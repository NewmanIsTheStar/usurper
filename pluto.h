
#ifndef PLUTO_H
#define PLUTO_H

#define PLUTO_VER "00.00.01"            //manually maintained version
#define USE_GIT_HASH_AS_VERSION (1)     //use git hash as version

#define STRNCPY(dst, src, size)  {strncpy((dst), (src), (size)); *((dst)+(size)-1)=0;}
#define STRNCAT(dst, src, size)  {if ((size) > 0) {*((dst)+(size)-1)=0; strncat((dst), (src), (size)-strlen((dst))-1);};}
// WARNING -- STRCAT macro can only be used when the destination size can be found with sizeof() -- if a pointer is the destination then use STRNCAT
#define STRCAT(dst, src)  {*((dst)+(sizeof(dst))-1)=0; strncat((dst), (src), (sizeof(dst))-strlen((dst))-1);}
#define NUM_ROWS(x) (sizeof(x)/sizeof(x[0]))
#define MASKED_WRITE(dest,src,mask) {(dest) = (((dest) & (~(mask))) | ((src) & (mask)));}
#define CLIP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

//int applet_entry_point(void);
void hex_dump(const uint8_t *bptr, uint32_t len);
int application_restart(void);

#endif 
