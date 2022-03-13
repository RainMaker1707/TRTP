#include "packet_interface.h"
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <math.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdlib.h>
#include <netinet/in.h> 
#include <sys/types.h> 
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>

/* Extra #includes */
/* Your code will be inserted here */

struct __attribute__((__packed__)) pkt {
    ptypes_t type;
    uint32_t crc1, crc2, timestamp;
    uint16_t length;
    uint8_t window, seqnum, tr;
    char *payload;
};

/* Extra code */
/* Your code will be inserted here */

pkt_t* pkt_new(){
    pkt_t *pkt = (pkt_t*) malloc(sizeof(pkt_t));
    return pkt;
}

void pkt_del(pkt_t *pkt){
    if(pkt_get_tr(pkt) == 0 && pkt_get_type(pkt) == PTYPE_DATA) free(pkt->payload);
    free(pkt);
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt){
    // TR & TYPE & WINDOW
    if(len == 0) return -1;
    size_t offset = 0;
    uint8_t tr = data[offset];
    tr=tr << 2;
    tr=tr >> 7;
    pkt_set_tr(pkt,((uint8_t)tr));
    uint8_t type = (uint8_t)data[offset];
    ptypes_t type_ = type >> 6;
    pkt_set_type(pkt, type_);
    uint8_t window = data[offset++];
    window = window << 3;
    window = window >> 3;
    pkt_set_window(pkt,((uint8_t)(window)));
    if(pkt->tr==1 && (pkt->type== PTYPE_ACK || pkt->type== PTYPE_NACK)) return E_TYPE;
    int header;
    if(pkt->type == PTYPE_DATA) header = 8;
    else header = 6;
    // LEN & SEQ
    if(pkt_get_type(pkt) == PTYPE_DATA || pkt_get_type(pkt)== PTYPE_FEC){
        uint8_t  l1 = data[offset++]; 
        uint8_t  l2 = data[offset++]; 
        uint16_t len_ = ((uint16_t)l1 << 8) | l2;
        if(len_ > 512) return E_LENGTH;
        pkt_set_length(pkt, len_);
    }else offset += 2;
    pkt_set_seqnum(pkt, data[offset++]);
    // TIMESTAMP
    uint8_t time[4];
    time[0] = data[offset++];
    time[1] = data[offset++];
    time[2] = data[offset++];
    time[3] = data[offset++];
    uint32_t timestamp = ntohl((uint32_t)(((((time[0] << 8) | time[1]) << 8) | time[2]) << 8 ) | time[3]);
    pkt_set_timestamp(pkt, timestamp);
    // CRC & PAYLOAd
    uint8_t crc[4];
    crc[0] = data[offset++];
    crc[1] = data[offset++];
    crc[2] = data[offset++];
    crc[3] = data[offset++];
    uint32_t CRC = (uint32_t)(((((crc[0] << 8) | crc[1]) << 8) | crc[2]) << 8 ) | crc[3];
    pkt_set_crc1(pkt, CRC);
    if (pkt_get_type(pkt) == PTYPE_DATA || pkt_get_type(pkt) == PTYPE_FEC){
        pkt_set_payload(pkt, data+offset, pkt_get_length(pkt));
        offset += pkt_get_length(pkt);
    }
    if(pkt_get_type(pkt) == PTYPE_ACK) pkt_set_payload(pkt, NULL, 0);
    if(pkt_get_type(pkt) == PTYPE_DATA){
        uint8_t crc2[4];
        crc2[0] = data[offset++];
        crc2[1] = data[offset++];
        crc2[2] = data[offset++];
        crc2[3] = data[offset++];
        uint32_t CRC2 = (uint32_t)(((((crc2[0] << 8) | crc2[1]) << 8) | crc2[2]) << 8 ) | crc2[3];
        pkt_set_crc2(pkt,CRC2);
    }
    char *data_ = (char*) malloc(sizeof(char)*header);
    if(!data_) return E_NOMEM;
    data_[0] = data[0] & 0b11011111;
    for(int i = 1; i < header; i++) data_[i] = data[i];
    uint32_t check = htonl(crc32(0, Z_NULL, 0));
    check = htonl(crc32(check, (Bytef*)data_, header));
    free(data_);
    return PKT_OK;
}
pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len){
    /* Your code will be inserted here */
    if(*len < sizeof(pkt)) return E_NOMEM;

    // define useful variables
    int header;
    if(pkt_get_type(pkt) == PTYPE_DATA) header = 8;
    else header = 6;

    // TR & TYPE & WINDOW
    uint8_t tr = pkt_get_tr(pkt) << 5;
    uint8_t type = pkt_get_type(pkt) << 6;
    uint8_t window =pkt_get_window(pkt);
    uint8_t join = (type)|(tr)|(window);
    memcpy(buf, &join, sizeof(uint8_t));
    size_t offset = sizeof(uint8_t);
    // LEN
    uint16_t len_ =  pkt_get_length(pkt);
    uint16_t hton_len = htons(len_);
    memcpy(buf + offset, &hton_len, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    // SEQ & TIMESTAMP
    memcpy(buf + offset, &pkt->seqnum, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(buf + offset, &(pkt->timestamp), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    // CRC
    char *CRCB = (char*) malloc(sizeof(char) * offset);
    if(!CRCB) return E_NOMEM;
    CRCB[0] = buf[0] & 0b11011111;
    for(int i = 1; i < header; i++) CRCB[i] = buf[i];
    uint32_t CRC = crc32(0, Z_NULL, 0);
    CRC = htonl(crc32(CRC, (Bytef*)CRCB, offset));
    memcpy(buf + offset, &CRC, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    free(CRCB);
    // PAYLOAD
    if(pkt->payload && pkt_get_tr(pkt)==0){
        memcpy(buf + offset, pkt->payload, pkt_get_length(pkt));
        offset += pkt_get_length(pkt);
        uint32_t CRC2 = crc32(0, Z_NULL, 0);
        CRC2 = htonl(crc32(CRC2, (Bytef*)pkt->payload, pkt_get_length(pkt)));
        memcpy(buf + offset, &CRC2, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    // Set final len
    *len = offset;
    return PKT_OK;
}

ptypes_t pkt_get_type  (const pkt_t* pkt){return pkt->type;}

uint8_t  pkt_get_tr(const pkt_t* pkt){return pkt->tr;}

uint8_t  pkt_get_window(const pkt_t* pkt){return pkt->window;}

uint8_t  pkt_get_seqnum(const pkt_t* pkt){return pkt->seqnum;}

uint16_t pkt_get_length(const pkt_t* pkt){return (uint16_t)pkt->length;}

uint32_t pkt_get_timestamp   (const pkt_t* pkt){return pkt->timestamp;}

uint32_t pkt_get_crc1   (const pkt_t* pkt){return pkt->crc1;}

uint32_t pkt_get_crc2   (const pkt_t* pkt){return pkt->crc2;}

const char* pkt_get_payload(const pkt_t* pkt){return pkt->payload;}

pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type){
    pkt->type=type;
    return (PKT_OK);
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr){
    pkt->tr=tr;
    return (PKT_OK);
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window){
    if (window>MAX_WINDOW_SIZE) return E_WINDOW;
    pkt->window=window;
    return (PKT_OK);
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum){
 pkt->seqnum=seqnum;
 return (PKT_OK);
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length){
 pkt->length=(length);
 return (PKT_OK);
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp){
    pkt->timestamp=timestamp;
    return(PKT_OK);
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1){
    pkt->crc1=crc1;
    return(PKT_OK);
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2){
    pkt->crc2=crc2;
    return(PKT_OK);
}


pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length){
  pkt->payload=calloc(1, (sizeof(char*) * length));
    if(pkt->payload== NULL) return (E_NOMEM);
    memcpy(pkt->payload, data, length);
    pkt_set_length(pkt, length);
    return (PKT_OK);
}



int main(int argc, char *argv[]){
    pkt_t *packet = (pkt_t*)malloc(sizeof(pkt_t));
    char *payload = "Hello, i'm the payload !";
    printf("%s: %d\n", payload, 25);

    size_t *len = malloc(sizeof(size_t));
    *len = 1024;
    pkt_set_type(packet, PTYPE_ACK);
    pkt_set_tr(packet, 0);
    pkt_set_window(packet, 31);
    pkt_set_length(packet, sizeof(payload));
    pkt_set_payload(packet, payload, sizeof(char) * 512);
    pkt_set_seqnum(packet, 123);

    char *buffer = malloc(sizeof(char) * *len);
    pkt_encode(packet, buffer, len);
    pkt_t *decoded = (pkt_t*)malloc(sizeof(pkt_t));
    pkt_decode(buffer, *len, decoded);
    printf("decoded: \n\tpkt->type: %d\n\tpkt->tr:%d\n\tpkt->window: %d", decoded->type, decoded->tr, decoded->window);
    printf("\n\tpkt->seqnum: %d\n\tpkt->length: %d\n\tpkt->payload: %s\n", decoded->seqnum, decoded->length, decoded->payload);
    return  EXIT_SUCCESS;
}