/*
 * BSD-3-Clause License
 *
 * Copyright (c) 2016, Matt Redfearn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <common/uart.h>

#define PRINT_BUF_LEN 64

typedef __builtin_va_list va_list;
#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#define va_copy(d,s)    __builtin_va_copy(d,s)

static void simple_outputchar(char **str, char c)
{
	if (str) {
		**str = c;
		++(*str);
	} else {
		uart_send(c);
	}
}

enum flags {
	PAD_ZERO = 1,
	PAD_RIGHT = 2
};

//将要打印的字符string放入打印位置out中，如果string的长度大于out，那么string内多余的字符会调用uart_send函数处理。
//out可以看成是输出缓冲区
//flags两个选项，补零或者是右对齐。
//补零：假设width=10，打印的数字是12345，那么实际输出是0000012345；如果打印的数字是-12345，那么实际输出是-000012345，注意，'-'号也占一个位置，所有后者比前者少数出一个0.
//右对齐：width=10,string=12345,output=12345      后面跟着五个空格
//如果string超过width的长度，那么什么都不补，不管了
static int prints(char **out, const char *string, int width, int flags)
{
	int pc = 0, padchar = ' ';

	if (width > 0) {
		int len = 0;
		const char *ptr;
		for (ptr = string; *ptr; ++ptr)
			++len;
		if (len >= width)
			width = 0;
		else
			width -= len;
		if (flags & PAD_ZERO)
			padchar = '0';
	}
	if (!(flags & PAD_RIGHT)) {
		for (; width > 0; --width) {
			simple_outputchar(out, padchar);
			++pc;
		}
	}
	for (; *string; ++string) {
		simple_outputchar(out, *string);
		++pc;
	}
	for (; width > 0; --width) {
		simple_outputchar(out, padchar);
		++pc;
	}

	return pc;
}

// this function print number `i` in the base of `base` (base > 1)
// `sign` is the flag of print signed number or unsigned number
// `width` and `flags` mean the length of printed number at least `width`,
// if the length of number is less than `width`, choose PAD_ZERO or PAD_RIGHT
// `letbase` means uppercase('A') or lowercase('a') when using hex
// you may need to call `prints`
// you do not need to print prefix like "0x", "0"...
// Remember the most significant digit is printed first.
static int printk_write_num(char **out, long long i, int base, int sign,
			    int width, int flags, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	//这里将有符号数转为无符号数,如果i是负数,那么此时会变成很大的正数
	unsigned long long u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints(out, print_buf, width, flags);
	}

	if (sign && base == 10 && i < 0) {
		neg = 1;
		//将负数变成正数,从很大的正数(比如-1对应的无符号数)变成对应的绝对值(也就是1)
		u = -i;
	}
	// TODO: fill your code here
	// store the digitals in the buffer `print_buf`:
	// 1. the last postion of this buffer must be '\0'
	// 2. the format is only decided by `base` and `letbase` here

	s = print_buf + PRINT_BUF_LEN;
	*--s = '\0';
	while(u > 0){
		s--;
		t = u % base;
		if(t < 10){
			*s = t + '0';
		}
		else{
			*s = t - 10 + letbase;
		}
		u /= base;
	}
	//这里有个小问题，转换的数字长度可能会长于print_buf_len-1，s可能会超过系统分配给他的范围，不过暂时没问题就先不改了。
	//可能还存在一个问题，如果base特别大，比如10000，那么可能t的值会超过一个字符的大小，然后出错。不过这里是static函数，只要保证外层调用时只提供某些进制的转换即可。

	if (neg) {
		if (width && (flags & PAD_ZERO)) {
			simple_outputchar(out, '-');
			++pc;
			--width;
		} else {
			*--s = '-';
		}
	}


	return pc + prints(out, s, width, flags);
}

static int simple_vsprintf(char **out, const char *format, va_list ap)
{
	int width, flags;
	int pc = 0;
	char scr[2];
	union {
		char c;
		char *s;
		int i;
		unsigned int u;
		long li;
		unsigned long lu;
		long long lli;
		unsigned long long llu;
		short hi;
		unsigned short hu;
		signed char hhi;
		unsigned char hhu;
		void *p;
	} u;

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = flags = 0;
			if (*format == '\0')
				break;
			if (*format == '%')
				goto out;
			if (*format == '-') {
				++format;
				flags = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				flags |= PAD_ZERO;
			}
			if (*format == '*') {
				width = va_arg(ap, int);
				format++;
			} else {
				for (; *format >= '0' && *format <= '9';
				     ++format) {
					width *= 10;
					width += *format - '0';
				}
			}
			switch (*format) {
			case ('d'):
				u.i = va_arg(ap, int);
				pc +=
				    printk_write_num(out, u.i, 10, 1, width,
						     flags, 'a');
				break;

			case ('u'):
				u.u = va_arg(ap, unsigned int);
				pc +=
				    printk_write_num(out, u.u, 10, 0, width,
						     flags, 'a');
				break;

			case ('o'):
				u.u = va_arg(ap, unsigned int);
				pc +=
				    printk_write_num(out, u.u, 8, 0, width,
						     flags, 'a');
				break;

			case ('x'):
				u.u = va_arg(ap, unsigned int);
				pc +=
				    printk_write_num(out, u.u, 16, 0, width,
						     flags, 'a');
				break;

			case ('X'):
				u.u = va_arg(ap, unsigned int);
				pc +=
				    printk_write_num(out, u.u, 16, 0, width,
						     flags, 'A');
				break;

			case ('p'):
				u.lu = va_arg(ap, unsigned long);
				pc +=
				    printk_write_num(out, u.lu, 16, 0, width,
						     flags, 'a');
				break;

			case ('c'):
				u.c = va_arg(ap, int);
				scr[0] = u.c;
				scr[1] = '\0';
				pc += prints(out, scr, width, flags);
				break;

			case ('s'):
				u.s = va_arg(ap, char *);
				pc +=
				    prints(out, u.s ? u.s : "(null)", width,
					   flags);
				break;
			case ('l'):
				++format;
				switch (*format) {
				case ('d'):
					u.li = va_arg(ap, long);
					pc +=
					    printk_write_num(out, u.li, 10, 1,
							     width, flags, 'a');
					break;

				case ('u'):
					u.lu = va_arg(ap, unsigned long);
					pc +=
					    printk_write_num(out, u.lu, 10, 0,
							     width, flags, 'a');
					break;

				case ('o'):
					u.lu = va_arg(ap, unsigned long);
					pc +=
					    printk_write_num(out, u.lu, 8, 0,
							     width, flags, 'a');
					break;

				case ('x'):
					u.lu = va_arg(ap, unsigned long);
					pc +=
					    printk_write_num(out, u.lu, 16, 0,
							     width, flags, 'a');
					break;

				case ('X'):
					u.lu = va_arg(ap, unsigned long);
					pc +=
					    printk_write_num(out, u.lu, 16, 0,
							     width, flags, 'A');
					break;

				case ('l'):
					++format;
					switch (*format) {
					case ('d'):
						u.lli = va_arg(ap, long long);
						pc +=
						    printk_write_num(out, u.lli,
								     10, 1,
								     width,
								     flags,
								     'a');
						break;

					case ('u'):
						u.llu =
						    va_arg(ap,
							   unsigned long long);
						pc +=
						    printk_write_num(out, u.llu,
								     10, 0,
								     width,
								     flags,
								     'a');
						break;

					case ('o'):
						u.llu =
						    va_arg(ap,
							   unsigned long long);
						pc +=
						    printk_write_num(out, u.llu,
								     8, 0,
								     width,
								     flags,
								     'a');
						break;

					case ('x'):
						u.llu =
						    va_arg(ap,
							   unsigned long long);
						pc +=
						    printk_write_num(out, u.llu,
								     16, 0,
								     width,
								     flags,
								     'a');
						break;

					case ('X'):
						u.llu =
						    va_arg(ap,
							   unsigned long long);
						pc +=
						    printk_write_num(out, u.llu,
								     16, 0,
								     width,
								     flags,
								     'A');
						break;

					default:
						break;
					}
					break;
				default:
					break;
				}
				break;
			case ('h'):
				++format;
				switch (*format) {
				case ('d'):
					u.hi = va_arg(ap, int);
					pc +=
					    printk_write_num(out, u.hi, 10, 1,
							     width, flags, 'a');
					break;

				case ('u'):
					u.hu = va_arg(ap, unsigned int);
					pc +=
					    printk_write_num(out, u.lli, 10, 0,
							     width, flags, 'a');
					break;

				case ('o'):
					u.hu = va_arg(ap, unsigned int);
					pc +=
					    printk_write_num(out, u.lli, 8, 0,
							     width, flags, 'a');
					break;

				case ('x'):
					u.hu = va_arg(ap, unsigned int);
					pc +=
					    printk_write_num(out, u.lli, 16, 0,
							     width, flags, 'a');
					break;

				case ('X'):
					u.hu = va_arg(ap, unsigned int);
					pc +=
					    printk_write_num(out, u.lli, 16, 0,
							     width, flags, 'A');
					break;

				case ('h'):
					++format;
					switch (*format) {
					case ('d'):
						u.hhi = va_arg(ap, int);
						pc +=
						    printk_write_num(out, u.hhi,
								     10, 1,
								     width,
								     flags,
								     'a');
						break;

					case ('u'):
						u.hhu =
						    va_arg(ap, unsigned int);
						pc +=
						    printk_write_num(out, u.lli,
								     10, 0,
								     width,
								     flags,
								     'a');
						break;

					case ('o'):
						u.hhu =
						    va_arg(ap, unsigned int);
						pc +=
						    printk_write_num(out, u.lli,
								     8, 0,
								     width,
								     flags,
								     'a');
						break;

					case ('x'):
						u.hhu =
						    va_arg(ap, unsigned int);
						pc +=
						    printk_write_num(out, u.lli,
								     16, 0,
								     width,
								     flags,
								     'a');
						break;

					case ('X'):
						u.hhu =
						    va_arg(ap, unsigned int);
						pc +=
						    printk_write_num(out, u.lli,
								     16, 0,
								     width,
								     flags,
								     'A');
						break;

					default:
						break;
					}
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		} else {
 out:
			simple_outputchar(out, *format);
			++pc;
		}
	}
	if (out)
		**out = '\0';
	return pc;
}

void printk(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	simple_vsprintf(NULL, fmt, va);
	va_end(va);
}

void break_point()
{
	printk("[ChCore] Lab stalling ... \n");
}
