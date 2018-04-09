//
//  publicDef.h
//  PluginPro
//
//  Created by  on 13-12-13.
//  Copyright (c) 2013年 __MyCompanyName__. All rights reserved.
//

#ifndef PluginPro_publicDef_h
#define PluginPro_publicDef_h

#define PLUGINNAME_MAX_PATH                    260
typedef struct _PLUGIN_ALIAS
{
	int dwPluginNumber;			            // 1、2、3、4
	unsigned short wzPluginName[PLUGINNAME_MAX_PATH];	// XXX.dat
    
}PLUGIN_ALIAS, *PPLUGIN_ALIAS;

struct utf8_table {
	int cmask;
	int cval;
	int shift;
	long lmask;
	long lval;
};

static struct utf8_table utf8_table[] = {
	{0x80, 0x00, 0 * 6, 0x7F, 0, /* 1 byte sequence */ },
	{0xE0, 0xC0, 1 * 6, 0x7FF, 0x80, /* 2 byte sequence */ },
	{0xF0, 0xE0, 2 * 6, 0xFFFF, 0x800, /* 3 byte sequence */ },
	{0xF8, 0xF0, 3 * 6, 0x1FFFFF, 0x10000, /* 4 byte sequence */ },
	{0xFC, 0xF8, 4 * 6, 0x3FFFFFF, 0x200000, /* 5 byte sequence */ },
	{0xFE, 0xFC, 5 * 6, 0x7FFFFFFF, 0x4000000, /* 6 byte sequence */ },
	{0, /* end of table    */ }
};

int Unicode_Character_to_UTF8_Character(uint8_t * s, uint16_t wc, int maxlen)
{
	long l;
	int c, nc;
	struct utf8_table *t;
	
	if (s == 0)
		return 0;
	
	l = wc;
	nc = 0;
	for (t = utf8_table; t->cmask && maxlen; t++, maxlen--) {
		nc++;
		if (l <= t->lmask) {
			c = t->shift;
			*s = t->cval | (l >> c);
			while (c > 0) {
				c -= 6;
				s++;
				*s = 0x80 | ((l >> c) & 0x3F);
			}
			return nc;
		}
	}
	return -1;
}

//maxlen pwcs的字节长度 utf8内存分配大小strlen
int Unicode_String_to_UTF8_String(uint8_t * s, const uint16_t * pwcs, int maxlen)
{
	const uint16_t *ip;
	uint8_t *op;
	int size;
	
	op = s;
	ip = pwcs;
	while (*ip && maxlen > 0) {
		if (*ip > 0x7f) {
			size = Unicode_Character_to_UTF8_Character(op, *ip, maxlen);
			if (size == -1) {
				/* Ignore character and move on */
				maxlen--;
			} else {
				op += size;
				maxlen -= size;
			}
		} else {
			*op++ = (uint8_t) * ip;
			maxlen--;
		}
		ip++;
	}
	return (op - s);
}

//返回转化的utf8字节数
int UTF8_Character_To_Unicode_Character(uint16_t * p, const uint8_t * s, int maxLen)
{
	long l;
	int c0, c, nc;
	struct utf8_table *t;
	
	nc = 0;
	c0 = *s;
	l = c0;
	for (t = utf8_table; t->cmask; t++) {
		nc++;
		if ((c0 & t->cmask) == t->cval) {
			l &= t->lmask;
			if (l < t->lval)
				return -1;
			*p = l;
			return nc;
		}
		if (maxLen <= nc)
			return -1;
		s++;
		c = (*s ^ 0x80) & 0xFF;
		if (c & 0xC0)
			return -1;
		l = (l << 6) | c;
	}
	return -1;
}
//maxLen为utf8字节数，pwcs应该分配strlen(utf8)*2
int UTF8_String_To_Unicode_String(uint16_t * pwcs, const uint8_t * s, int maxLen)
{
	uint16_t *op;
	const uint8_t *ip;
	int size;
	
	op = pwcs;
	ip = s;
	while (*ip && maxLen > 0) {
		
		if (*ip & 0x80) {
			size = UTF8_Character_To_Unicode_Character(op, ip, maxLen);
			if (size == -1) {
				/* Ignore character and move on */
				ip++;
				maxLen--;
			} else {
				op += 1;
				ip += size;
				maxLen -= size;
			}
		} else {
			*op++ = *ip++;
		}
	}
	return (op - pwcs);
}
int utf8_count(char *str){
	int i = 0;
	while (str[i] > 0)
		i++;
	
	if (str[i] <= -65) // all follower bytes have values below -65
		return -1; // invalid
    
	int count = i;
	while (str[i])
	{
		//if ASCII just go to next character
		if (str[i] > 0)      i += 1;
		else
			//select amongst multi-byte starters
			switch (0xF0 & str[i])
		{
			case 0xE0: i += 3; break;
			case 0xF0: i += 4; break;
			default:   i += 2; break;
		}
		++count;
	}
	return count;
}

#endif
