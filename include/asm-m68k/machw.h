/*
** linux/machw.h -- This header defines some macros and pointers for
**                    the various Macintosh custom hardware registers.
**
** Copyright 1997 by Michael Schmitz
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
*/

#ifndef _ASM_MACHW_H_
#define _ASM_MACHW_H_

/*
 * head.S maps the videomem to VIDEOMEMBASE
 */

#define VIDEOMEMBASE	0xf0000000
#define VIDEOMEMSIZE	(4096*1024)
#define VIDEOMEMMASK	(-4096*1024)

#ifndef __ASSEMBLY__

#include <linux/types.h>

#if 0
/* Mac SCSI Controller 5380 */

#define	MAC_5380_BAS	(0x50F10000) /* This is definitely wrong!! */
struct MAC_5380 {
	u_char	scsi_data;
	u_char	char_dummy1;
	u_char	scsi_icr;
	u_char	char_dummy2;
	u_char	scsi_mode;
	u_char	char_dummy3;
	u_char	scsi_tcr;
	u_char	char_dummy4;
	u_char	scsi_idstat;
	u_char	char_dummy5;
	u_char	scsi_dmastat;
	u_char	char_dummy6;
	u_char	scsi_targrcv;
	u_char	char_dummy7;
	u_char	scsi_inircv;
};
#define	mac_scsi       ((*(volatile struct MAC_5380 *)MAC_5380_BAS))

/*
** SCC Z8530
*/

#define MAC_SCC_BAS (0x50F04000)
struct MAC_SCC
 {
  u_char cha_a_ctrl;
  u_char char_dummy1;
  u_char cha_a_data;
  u_char char_dummy2;
  u_char cha_b_ctrl;
  u_char char_dummy3;
  u_char cha_b_data;
 };
# define mac_scc ((*(volatile struct SCC*)MAC_SCC_BAS))
#endif

#endif /* __ASSEMBLY__ */

#endif /* linux/machw.h */
