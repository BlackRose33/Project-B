#include <asm/byteorder.h>

static unsigned short do_csum(const unsigned char *buff, int len)
{
  register unsigned long sum = 0;
  int swappem = 0;

  if (1 & (unsigned long)buff) {
    sum = *buff << 8;
    buff++;
    len--;
    ++swappem;
  }

  while (len > 1) {
    sum += *(unsigned short *)buff;
    buff += 2;
    len -= 2;
  }

  if (len > 0)
    sum += *buff;

  /*  Fold 32-bit sum to 16 bits */
  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);

  if (swappem)
    sum = ((sum & 0xff00) >> 8) + ((sum & 0x00ff) << 8);

  return sum;

}

__sum16 ip_compute_csum(const void *buff, int len)
{
  return (__force __sum16)~do_csum(buff, len);
}