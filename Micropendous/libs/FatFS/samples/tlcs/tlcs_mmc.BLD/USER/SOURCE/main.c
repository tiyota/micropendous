/***********************************************************************/
/* FatFs Test Monitor for TLCS-870/C                                   */
/***********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "integer.h"
#include "Io86fm29.h"
#include "monitor.h"
#include "ff.h"
#include "diskio.h"




DWORD acc_size;				/* Work register for fs command */
WORD acc_files, acc_dirs;
FILINFO Finfo;

char Line[100];				/* Console input buffer */
BYTE Buff[512];				/* Working buffer */

FATFS fatfs;				/* File system object */

const BYTE samurai[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


/* 100Hz increment timer */
volatile WORD Timer;

/* Real Time Clock */
volatile BYTE rtcYear = 106, rtcMon = 6, rtcMday = 1, rtcHour, rtcMin, rtcSec;



/*---------------------------------------------------------*/
/* User Provided Timer Function for FatFs module           */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support a real time clock.          */


DWORD get_fattime ()
{
	DWORD tmr;


	__DI();
	tmr =	  (((DWORD)rtcYear - 80) << 25)
			| ((DWORD)rtcMon << 21)
			| ((DWORD)rtcMday << 16)
			| (WORD)(rtcHour << 11)
			| (WORD)(rtcMin << 5)
			| (WORD)(rtcSec >> 1);
	__EI();

	return tmr;
}


void __interrupt Int_TBT()
{
	BYTE n;


	WDTCR2=0x4E;

	if (++rtcSec >= 60) {
		rtcSec = 0;
		if (++rtcMin >= 60) {
			rtcMin = 0;
			if (++rtcHour >= 24) {
				rtcHour = 0;
				n = samurai[rtcMon - 1];
				if ((n == 28) && !(rtcYear & 3)) n++;
				if (++rtcMday > n) {
					rtcMday = 1;
					if (++rtcMon > 12) {
						rtcMon = 1;
						rtcYear++;
					}
				}
			}
		}
	}
}


void __interrupt Int_TC4()
{
	Timer++;
	disk_timerproc();
}



/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */



static
void put_dump (BYTE *buff, DWORD ofs, BYTE cnt)
{
	BYTE n;


	xprintf("%08lX ", ofs);
	for(n = 0; n < cnt; n++)
		xprintf(" %02X", buff[n]);
	xputc(' ');
	for(n = 0; n < cnt; n++) {
		if ((buff[n] < 0x20)||(buff[n] >= 0x7F))
			xputc('.');
		else
			xputc(buff[n]);
	}
	xputc('\n');
}


static
void put_rc (FRESULT rc)
{
	const char *p;
	static const char str[] =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0";
	FRESULT i;

	for (p = str, i = 0; i != rc && *p; i++) {
		while(*p++);
	}
	xprintf("rc=%u FR_%s\n", (WORD)rc, p);
}


static
FRESULT scan_files (char* path)
{
	DIR dirs;
	FRESULT res;
	BYTE i;


	if ((res = f_opendir(&dirs, path)) == FR_OK) {
		i = strlen(path);
		while (((res = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {
			if (Finfo.fattrib & AM_DIR) {
				acc_dirs++;
				*(path+i) = '/'; strcpy(path+i+1, &Finfo.fname[0]);
				res = scan_files(path);
				*(path+i) = '\0';
				if (res != FR_OK) break;
			} else {
				acc_files++;
				acc_size += Finfo.fsize;
			}
		}
	}

	return res;
}



/*-----------------------------------------------------------------------*/
/* Main                                                                  */


void main ()
{
	BYTE res, b1;
	char *ptr, *ptr2;
	long p1, p2, p3;
	WORD w1;
	DWORD dw, ofs, sect = 0;
	UINT s1, s2, cnt, blen = sizeof(Buff);
	DIR dir;				/* Directory object */
	FIL file1, file2;		/* File object */
	FATFS *fs;



	xputs("\nFatFs module test monitor\n");

	for (;;) {
		xputc('>');
		ptr = Line;
		get_line(ptr, sizeof(Line));

		switch (*ptr++) {

		case 'd' :
			switch (*ptr++) {
			case 'd' :	/* dd [<sector>] - Dump secrtor */
				if (!xatoi(&ptr, &p1))
					p1 = sect;
				res = disk_read(0, Buff, p1, 1);
				if (res) { xprintf("rc=%u\n", (WORD)res); break; }
				sect = p1 + 1;
				xprintf("Sector:%lu\n", p1);
				for (ptr = (char*)Buff, ofs = 0; ofs < 0x200; ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16);
				break;

			case 'i' :	/* di - Initialize disk */
				xprintf("rc=%u\n", (WORD)disk_initialize(0));
				break;

			case 's' :	/* ds - Show disk status */
				if (disk_ioctl(0, GET_SECTOR_COUNT, &p2) == RES_OK)
					{ xprintf("Drive size: %lu sectors\n", p2); }
				if (disk_ioctl(0, GET_SECTOR_SIZE, &w1) == RES_OK)
					{ xprintf("Sector size: %u\n", w1); }
				if (disk_ioctl(0, GET_BLOCK_SIZE, &p2) == RES_OK)
					{ xprintf("Erase block size: %lu sectors\n", p2); }
				if (disk_ioctl(0, MMC_GET_TYPE, &b1) == RES_OK)
					{ xprintf("MMC/SDC type: %u\n", b1); }
				if (disk_ioctl(0, MMC_GET_CSD, Buff) == RES_OK)
					{ xputs("CSD:\n"); put_dump(Buff, 0, 16); }
				if (disk_ioctl(0, MMC_GET_CID, Buff) == RES_OK)
					{ xputs("CID:\n"); put_dump(Buff, 0, 16); }
				if (disk_ioctl(0, MMC_GET_OCR, Buff) == RES_OK)
					{ xputs("OCR:\n"); put_dump(Buff, 0, 4); }
				if (disk_ioctl(0, MMC_GET_SDSTAT, Buff) == RES_OK) {
					xputs("SD Status:\n");
					for (s1 = 0; s1 < 64; s1 += 16) put_dump(Buff+s1, s1, 16);
				}
			}
			break;

		case 'b' :
			switch (*ptr++) {
			case 'd' :	/* bd <addr> - Dump R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				for (ptr=(char*)(&Buff[p1]), ofs = p1, cnt = 32; cnt; cnt--, ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16);
				break;

			case 'e' :	/* be <addr> [<data>] ... - Edit R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				if (xatoi(&ptr, &p2)) {
					do {
						Buff[p1++] = (BYTE)p2;
					} while (xatoi(&ptr, &p2));
					break;
				}
				for (;;) {
					xprintf("%04X %02X-", (WORD)(p1), (WORD)Buff[p1]);
					get_line(Line, sizeof(Line));
					ptr = Line;
					if (*ptr == '.') break;
					if (*ptr < ' ') { p1++; continue; }
					if (xatoi(&ptr, &p2))
						Buff[p1++] = (BYTE)p2;
					else
						xputs("???\n");
				}
				break;

			case 'r' :	/* br <sector> <n> - Read disk into R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) p2 = 1;
				xprintf("rc=%u\n", (WORD)disk_read(0, Buff, p1, (BYTE)p2));
				break;

			case 'w' :	/* bw <sector> <n> - Write R/W buffer into disk */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) p2 = 1;
				xprintf("rc=%u\n", (WORD)disk_write(0, Buff, p1, (BYTE)p2));
				break;

			case 'f' :	/* bf <n> - Fill R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				memset(Buff, (BYTE)p1, sizeof(Buff));
				break;

			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi - Force initialized the file system */
				put_rc(f_mount(0, &fatfs));
				break;

			case 's' :	/* fs - Show file system status */
				res = f_getfree("", (DWORD*)&p2, &fs);
				if (res) { put_rc(res); break; }
				xprintf("FAT type = %u\nBytes/Cluster = %lu\nNumber of FATs = %u\n"
						"Root DIR entries = %u\nSectors/FAT = %lu\nNumber of clusters = %lu\n"
						"FAT start (lba) = %lu\nDIR start (lba,clustor) = %lu\nData start (lba) = %lu\n",
						(WORD)fs->fs_type, (DWORD)fs->csize * 512, (WORD)fs->n_fats,
						fs->n_rootdir, (DWORD)fs->sects_fat, (DWORD)(fs->max_clust - 2),
						fs->fatbase, fs->dirbase, fs->database
				);
				acc_size = acc_files = acc_dirs = 0;
				res = scan_files(ptr);
				if (res) { put_rc(res); break; }
				xprintf("%u files, %lu bytes.\n%u folders.\n"
						"%luKi bytes total disk space.\n%luKi bytes available.\n",
						acc_files, acc_size, acc_dirs,
						(DWORD)(fs->max_clust - 2) * (fs->csize / 2), p2 * (fs->csize / 2)
				);

			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				res = f_opendir(&dir, ptr);
				if (res) { put_rc(res); break; }
				p1 = s1 = s2 = 0;
				for(;;) {
					res = f_readdir(&dir, &Finfo);
					if ((res != FR_OK) || !Finfo.fname[0]) break;
					if (Finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; p1 += Finfo.fsize;
					}
					xprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s\n", 
							(Finfo.fattrib & AM_DIR) ? 'D' : '-',
							(Finfo.fattrib & AM_RDO) ? 'R' : '-',
							(Finfo.fattrib & AM_HID) ? 'H' : '-',
							(Finfo.fattrib & AM_SYS) ? 'S' : '-',
							(Finfo.fattrib & AM_ARC) ? 'A' : '-',
							(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
							(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,
							Finfo.fsize, &(Finfo.fname[0]));
				}
				xprintf("%4u File(s),%10lu bytes\n%4u Dir(s)\n", s1, p1, s2);
				if (f_getfree("", (DWORD*)&p1, &fs) == FR_OK)
					xprintf(", %10luKi bytes free\n", p1 * fs->csize / 2);
				break;

			case 'o' :	/* fo <mode> <file> - Open a file */
				if (!xatoi(&ptr, &p1)) break;
				res = f_open(&file1, ptr, (BYTE)p1);
				xprintf("rc=%u\n", (WORD)res);
				break;

			case 'c' :	/* fc - Close a file */
				res = f_close(&file1);
				xprintf("rc=%u\n", (WORD)res);
				break;

			case 'e' :	/* fe - Seek file pointer */
				if (!xatoi(&ptr, &p1)) break;
				res = f_lseek(&file1, p1);
				put_rc(res);
				if (res == FR_OK)
					xprintf("fptr = %lu(0x%lX)\n", file1.fptr, file1.fptr);
				break;

			case 'r' :	/* fr <len> - read file */
				if (!xatoi(&ptr, &p1)) break;
				p2 = 0;
				Timer = 0;
				while (p1) {
					if (p1 >= blen)	{ cnt = blen; p1 -= blen; }
					else 			{ cnt = (WORD)p1; p1 = 0; }
					res = f_read(&file1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				s2 = Timer;
				xprintf("%lu bytes read with %lu bytes/sec.\n", p2, p2 * 100 / s2);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) break;
				ofs = file1.fptr;
				while (p1) {
					if (p1 >= 16)	{ cnt = 16; p1 -= 16; }
					else 			{ cnt = (WORD)p1; p1 = 0; }
					res = f_read(&file1, Buff, cnt, &cnt);
					if (res != FR_OK) { put_rc(res); break; }
					if (!cnt) break;
					put_dump(Buff, ofs, (BYTE)cnt);
					ofs += 16;
				}
				break;

			case 'w' :	/* fw <len> <val> - write file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				memset(Buff, (BYTE)p2, blen);
				p2 = 0;
				Timer = 0;
				while (p1) {
					if (p1 >= blen)	{ cnt = blen; p1 -= blen; }
					else 			{ cnt = (WORD)p1; p1 = 0; }
					res = f_write(&file1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				s2 = Timer;
				xprintf("%lu bytes written with %lu bytes/sec.\n", p2, p2 * 100 / s2);
				break;

			case 'v' :	/* fv - Truncate file */
				put_rc(f_truncate(&file1));
				break;

			case 'n' :	/* fn <old_name> <new_name> - Change file/dir name */
				while (*ptr == ' ') ptr++;
				ptr2 = strchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				put_rc(f_rename(ptr, ptr2));
				break;

			case 'u' :	/* fu <file> - Unlink a file or dir */
				while (*ptr == ' ') ptr++;
				put_rc(f_unlink(ptr));
				break;

			case 'k' :	/* fk <dir> - Create a directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_mkdir(ptr));
				break;

			case 'a' :	/* fa <atrr> <mask> <file> - Change file/dir attribute */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				while (*ptr == ' ') ptr++;
				put_rc(f_chmod(ptr, (BYTE)p1, (BYTE)p2));
				break;

			case 't' :	/* ft <year> <month> <day> <hour> <min> <sec> <name> */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.fdate = ((p1 - 1980) << 9) | ((p2 & 15) << 5) | (p3 & 31);
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.ftime = ((p1 & 31) << 11) | ((p2 & 63) << 5) | ((p3 >> 1) & 31);
				put_rc(f_utime(ptr, &Finfo));
				break;

			case 'x' : /* fx <src_name> <dst_name> - Copy file */
				while (*ptr == ' ') ptr++;
				ptr2 = strchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				if (!*ptr2) break;
				xprintf("Opening \"%s\"", ptr);
				res = f_open(&file1, ptr, (BYTE)(FA_OPEN_EXISTING | FA_READ));
				xputc('\n');
				if (res) {
					put_rc(res);
					break;
				}
				xprintf("Creating \"%s\"", ptr2);
				res = f_open(&file2, ptr2, (BYTE)(FA_CREATE_ALWAYS | FA_WRITE));
				xputc('\n');
				if (res) {
					put_rc(res);
					f_close(&file1);
					break;
				}
				xprintf("Copying...");
				p1 = 0;
				for (;;) {
					res = f_read(&file1, Buff, sizeof(Buff), &s1);
					if (res || s1 == 0) break;   /* error or eof */
					res = f_write(&file2, Buff, s1, &s2);
					p1 += s2;
					if (res || s2 < s1) break;   /* error or disk full */
				}
				xprintf("\n%lu bytes copied.\n", p1);
				f_close(&file1);
				f_close(&file2);
				break;

			case 'z' :	/* fz <len> - set transfer unit for fr/fw commands */
				if (xatoi(&ptr, &p1) && (p1 > 0) && (p1 <= sizeof(Buff)))
					blen = (WORD)p1;
				xprintf("%u\n", blen);
				break;
			}
			break;

		case 't' :	/* t [<year> <mon> <mday> <hour> <min> <sec>] */
			if (xatoi(&ptr, &p1)) {
				rtcYear = p1-1900;
				xatoi(&ptr, &p1); rtcMon = p1;
				xatoi(&ptr, &p1); rtcMday = p1;
				xatoi(&ptr, &p1); rtcHour = p1;
				xatoi(&ptr, &p1); rtcMin = p1;
				xatoi(&ptr, &p1); rtcSec = p1;
			}
			xprintf("%u/%u/%u %02u:%02u:%02u\n", (WORD)rtcYear+1900, rtcMon, rtcMday, rtcHour, rtcMin, rtcSec);
			break;
		}
	}

}


