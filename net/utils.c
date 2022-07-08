#include <net/utils.h>

uint32_t ntohl(uint32_t data)
{
  return (((data & 0x000000ff) << 24) |
          ((data & 0x0000ff00) << 8) |
          ((data & 0x00ff0000) >> 8) |
          ((data & 0xff000000) >> 24));
}

uint32_t htonl(uint32_t data)
{
  return ntohl(data);
}

uint16_t ntohs(uint16_t data)
{
  return (((data & 0x00ff) << 8) |
           (data & 0xff00) >> 8);
}

uint16_t htons(uint16_t data)
{
  return ntohs(data);
}
// call with checksum(hdr, hdr->ihl * 4, 0);
uint16_t checksum(void *addr, int count, int start_sum){
       /* Compute Internet Checksum for "count" bytes
        *         beginning at location "addr".
        * Taken from https://datatracker.ietf.org/doc/html/rfc1071#section-4.1
        */
   register uint32_t sum = start_sum;

   uint16_t * ptr = addr;

    while( count > 1 )  {
       /*  This is the inner loop */
           sum += * ptr++;
           count -= 2;
   }

       /*  Add left-over byte, if any */
   if( count > 0 )
           sum += *(uint8_t *) ptr;

       /*  Fold 32-bit sum to 16 bits */
   while (sum>>16)
       sum = (sum & 0xffff) + (sum >> 16);

   return ~sum;
}

uint16_t transport_checksum(uint32_t saddr, uint32_t daddr, uint8_t proto, char *data, uint16_t len)
{
    uint32_t sum = 0;


    sum += htonl(saddr);
    sum += htonl(daddr);
    sum += htons(proto);
    sum += len;
    
    return checksum(data, ntohs(len), sum);
}