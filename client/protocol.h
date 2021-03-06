#ifndef __UPLOAD_PROTOCOL_H
#define __UPLOAD_PROTOCOL_H

#define UPLOAD_CHUNK_SIZE       (10*1024)
//#define UPLOAD_CHUNK_SIZE     (1*1024)
//#define UPLOAD_CHUNK_SIZE     (128)

#define RESUME_TEMPLATE         ("\x05UPL,UID:%010u,CHECKSUM:%32s,FILESIZE:%020ld")
#define LEN_RESUME_TEMPLATE     (29+10+32+20)

#define RESUME_TEMPLATE_ACK     ("\x06UPL%08x")
#define LEN_RESUME_TEMPLATE_ACK (4+8)

#define CHUNK_HEAD_TEMPLATE     ("\x01CHUNKID%08x")
#define LEN_CHUNK_HEAD_TEMPLATE (8+8)

#endif
