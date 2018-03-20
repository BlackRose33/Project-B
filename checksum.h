#ifndef CHECKSUM_H
#define CHECKSUM_H

__sum16 ip_compute_csum(const void *buff, int len);

__sum16 ip_fast_csum(const void *iph, unsigned int ihl);

#endif
