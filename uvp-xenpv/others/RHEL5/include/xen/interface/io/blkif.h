/******************************************************************************
 * blkif.h
 * 
 * Unified block-device I/O interface for Xen guest OSes.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2003-2004, Keir Fraser
 */

#ifndef __XEN_PUBLIC_IO_BLKIF_H__
#define __XEN_PUBLIC_IO_BLKIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 * Front->back notifications: When enqueuing a new request, sending a
 * notification can be made conditional on req_event (i.e., the generic
 * hold-off mechanism provided by the ring macros). Backends must set
 * req_event appropriately (e.g., using RING_FINAL_CHECK_FOR_REQUESTS()).
 * 
 * Back->front notifications: When enqueuing a new response, sending a
 * notification can be made conditional on rsp_event (i.e., the generic
 * hold-off mechanism provided by the ring macros). Frontends must set
 * rsp_event appropriately (e.g., using RING_FINAL_CHECK_FOR_RESPONSES()).
 */

#ifndef blkif_vdev_t
#define blkif_vdev_t   uint16_t
#endif
#define blkif_sector_t uint64_t

/*
 * REQUEST CODES.
 */
#define BLKIF_OP_READ              0
#define BLKIF_OP_WRITE             1
/*
 * Recognised only if "feature-barrier" is present in backend xenbus info.
 * The "feature-barrier" node contains a boolean indicating whether barrier
 * requests are likely to succeed or fail. Either way, a barrier request
 * may fail at any time with BLKIF_RSP_EOPNOTSUPP if it is unsupported by
 * the underlying block-device hardware. The boolean simply indicates whether
 * or not it is worthwhile for the frontend to attempt barrier requests.
 * If a backend does not recognise BLKIF_OP_WRITE_BARRIER, it should *not*
 * create the "feature-barrier" node!
 */
#define BLKIF_OP_WRITE_BARRIER     2
/*
 * Recognised if "feature-flush-cache" is present in backend xenbus
 * info.  A flush will ask the underlying storage hardware to flush its
 * non-volatile caches as appropriate.  The "feature-flush-cache" node
 * contains a boolean indicating whether flush requests are likely to
 * succeed or fail. Either way, a flush request may fail at any time
 * with BLKIF_RSP_EOPNOTSUPP if it is unsupported by the underlying
 * block-device hardware. The boolean simply indicates whether or not it
 * is worthwhile for the frontend to attempt flushes.  If a backend does
 * not recognise BLKIF_OP_WRITE_FLUSH_CACHE, it should *not* create the
 * "feature-flush-cache" node!
 */
#define BLKIF_OP_FLUSH_DISKCACHE   3
/*
 * Device specific command packet contained within the request
 */
#define BLKIF_OP_PACKET            4
/*
 * Recognised only if "feature-discard" is present in backend xenbus info.
 * The "feature-discard" node contains a boolean indicating whether trim
 * (ATA) or unmap (SCSI) - conviently called discard requests are likely
 * to succeed or fail. Either way, a discard request
 * may fail at any time with BLKIF_RSP_EOPNOTSUPP if it is unsupported by
 * the underlying block-device hardware. The boolean simply indicates whether
 * or not it is worthwhile for the frontend to attempt discard requests.
 * If a backend does not recognise BLKIF_OP_DISCARD, it should *not*
 * create the "feature-discard" node!
 *
 * Discard operation is a request for the underlying block device to mark
 * extents to be erased. However, discard does not guarantee that the blocks
 * will be erased from the device - it is just a hint to the device
 * controller that these blocks are no longer in use. What the device
 * controller does with that information is left to the controller.
 * Discard operations are passed with sector_number as the
 * sector index to begin discard operations at and nr_sectors as the number of
 * sectors to be discarded. The specified sectors should be discarded if the
 * underlying block device supports trim (ATA) or unmap (SCSI) operations,
 * or a BLKIF_RSP_EOPNOTSUPP  should be returned.
 * More information about trim/unmap operations at:
 * http://t13.org/Documents/UploadedDocuments/docs2008/
 *     e07154r6-Data_Set_Management_Proposal_for_ATA-ACS2.doc
 * http://www.seagate.com/staticfiles/support/disc/manuals/
 *     Interface%20manuals/100293068c.pdf
 * The backend can optionally provide these extra XenBus attributes to
 * further optimize the discard functionality:
 * 'discard-aligment' - Devices that support discard functionality may
 * internally allocate space in units that are bigger than the exported
 * logical block size. The discard-alignment parameter indicates how many bytes
 * the beginning of the partition is offset from the internal allocation unit's
 * natural alignment. Do not confuse this with natural disk alignment offset.
 * 'discard-granularity'  - Devices that support discard functionality may
 * internally allocate space using units that are bigger than the logical block
 * size. The discard-granularity parameter indicates the size of the internal
 * allocation unit in bytes if reported by the device. Otherwise the
 * discard-granularity will be set to match the device's physical block size.
 * It is the minimum size you can discard.
 * 'discard-secure' - All copies of the discarded sectors (potentially created
 * by garbage collection) must also be erased.  To use this feature, the flag
 * BLKIF_DISCARD_SECURE must be set in the blkif_request_discard.
 */
#define BLKIF_OP_DISCARD           5

/*
 * Recognized if "feature-max-indirect-segments" in present in the backend
 * xenbus info. The "feature-max-indirect-segments" node contains the maximum
 * number of segments allowed by the backend per request. If the node is
 * present, the frontend might use blkif_request_indirect structs in order to
 * issue requests with more than BLKIF_MAX_SEGMENTS_PER_REQUEST (11). The
 * maximum number of indirect segments is fixed by the backend, but the
 * frontend can issue requests with any number of indirect segments as long as
 * it's less than the number provided by the backend. The indirect_grefs field
 * in blkif_request_indirect should be filled by the frontend with the
 * grant references of the pages that are holding the indirect segments.
 * This pages are filled with an array of blkif_request_segment_aligned
 * that hold the information about the segments. The number of indirect
 * pages to use is determined by the maximum number of segments
 * a indirect request contains. Every indirect page can contain a maximum
 * of 512 segments (PAGE_SIZE/sizeof(blkif_request_segment_aligned)),
 * so to calculate the number of indirect pages to use we have to do
 * ceil(indirect_segments/512).
 *
 * If a backend does not recognize BLKIF_OP_INDIRECT, it should *not*
 * create the "feature-max-indirect-segments" node!
 */
#define BLKIF_OP_INDIRECT          6
/*
 * Maximum scatter/gather segments per request.
 * This is carefully chosen so that sizeof(blkif_ring_t) <= PAGE_SIZE.
 * NB. This could be 12 if the ring indexes weren't stored in the same page.
 */
#define BLKIF_MAX_SEGMENTS_PER_REQUEST 11
#define BLKIF_MAX_INDIRECT_PAGES_PER_REQUEST 8

#ifdef CUSTOMIZED
struct blkif_request_segment {
    grant_ref_t gref;        /* reference to I/O buffer frame        */
    /* @first_sect: first sector in frame to transfer (inclusive).   */
    /* @last_sect: last sector in frame to transfer (inclusive).     */
    uint8_t     first_sect, last_sect;
};

struct blkif_request {
    uint8_t        operation;    /* BLKIF_OP_???                         */
    uint8_t        nr_segments;  /* number of segments                   */
    blkif_vdev_t   handle;       /* only for read/write requests         */
    uint64_t       id;           /* private guest value, echoed in resp  */
    blkif_sector_t sector_number;/* start sector idx on disk (r/w only)  */
    struct blkif_request_segment seg[BLKIF_MAX_SEGMENTS_PER_REQUEST];
};
#else
struct blkif_request_segment_aligned {
    grant_ref_t gref;        /* reference to I/O buffer frame        */
    /* @first_sect: first sector in frame to transfer (inclusive).   */
    /* @last_sect: last sector in frame to transfer (inclusive).     */
    uint8_t     first_sect, last_sect;
    uint16_t    _pad; /* padding to make it 8 bytes, so it's cache-aligned */
} __attribute__((__packed__));

struct blkif_request_rw {
    uint8_t        nr_segments;  /* number of segments                   */
    blkif_vdev_t   handle;       /* only for read/write requests         */
#ifndef CONFIG_X86_32
    uint32_t       _pad1;        /* offsetof(blkif_request,u.rw.id) == 8 */
#endif
    uint64_t       id;           /* private guest value, echoed in resp  */
    blkif_sector_t sector_number;/* start sector idx on disk (r/w only)  */
    struct blkif_request_segment {
        grant_ref_t gref;        /* reference to I/O buffer frame        */
        /* @first_sect: first sector in frame to transfer (inclusive).   */
        /* @last_sect: last sector in frame to transfer (inclusive).     */
        uint8_t     first_sect, last_sect;
    } seg[BLKIF_MAX_SEGMENTS_PER_REQUEST];
} __attribute__((__packed__));

struct blkif_request_discard {
    uint8_t        flag;         /* BLKIF_DISCARD_SECURE or zero.        */
#define BLKIF_DISCARD_SECURE (1<<0)  /* ignored if discard-secure=0          */
    blkif_vdev_t   _pad1;        /* only for read/write requests         */
#ifndef CONFIG_X86_32
    uint32_t       _pad2;        /* offsetof(blkif_req..,u.discard.id)==8*/
#endif
    uint64_t       id;           /* private guest value, echoed in resp  */
    blkif_sector_t sector_number;
    uint64_t       nr_sectors;
    uint8_t        _pad3;
} __attribute__((__packed__));

struct blkif_request_other {
    uint8_t      _pad1;
    blkif_vdev_t _pad2;        /* only for read/write requests         */
#ifndef CONFIG_X86_32
    uint32_t     _pad3;        /* offsetof(blkif_req..,u.other.id)==8*/
#endif
    uint64_t     id;           /* private guest value, echoed in resp  */
} __attribute__((__packed__));

struct blkif_request_indirect {
    uint8_t        indirect_op;
    uint16_t       nr_segments;
#ifndef CONFIG_X86_32
    uint32_t       _pad1;        /* offsetof(blkif_...,u.indirect.id) == 8 */
#endif
    uint64_t       id;
    blkif_sector_t sector_number;
    blkif_vdev_t   handle;
    uint16_t       _pad2;
    grant_ref_t    indirect_grefs[BLKIF_MAX_INDIRECT_PAGES_PER_REQUEST];
#ifndef CONFIG_X86_32
    uint32_t      _pad3;         /* make it 64 byte aligned */
#else
    uint64_t      _pad3;         /* make it 64 byte aligned */
#endif
} __attribute__((__packed__));
struct blkif_request {
    uint8_t        operation;    /* BLKIF_OP_???                         */
    union {
        struct blkif_request_rw rw;
        struct blkif_request_discard discard;
        struct blkif_request_other other;
        struct blkif_request_indirect indirect;
    } u;
} __attribute__((__packed__));
#endif

typedef struct blkif_request blkif_request_t;

struct blkif_response {
    uint64_t        id;              /* copied from request */
    uint8_t         operation;       /* copied from request */
    int16_t         status;          /* BLKIF_RSP_???       */
};
typedef struct blkif_response blkif_response_t;

/*
 * STATUS RETURN CODES.
 */
 /* Operation not supported (only happens on barrier writes). */
#define BLKIF_RSP_EOPNOTSUPP  -2
 /* Operation failed for some unspecified reason (-EIO). */
#define BLKIF_RSP_ERROR       -1
 /* Operation completed successfully. */
#define BLKIF_RSP_OKAY         0

/*
 * Generate blkif ring structures and types.
 */

DEFINE_RING_TYPES(blkif, struct blkif_request, struct blkif_response);

#define VDISK_CDROM        0x1
#define VDISK_REMOVABLE    0x2
#define VDISK_READONLY     0x4

#endif /* __XEN_PUBLIC_IO_BLKIF_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
