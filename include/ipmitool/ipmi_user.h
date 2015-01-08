/*
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef IPMI_USER_H
#define IPMI_USER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>

#define IPMI_USER_ENABLE_UNSPECIFIED 0x00
#define IPMI_USER_ENABLE_ENABLED 0x40
#define IPMI_USER_ENABLE_DISABLED 0x80
#define IPMI_USER_ENABLE_RESERVED 0xC0

/*
 * The GET USER ACCESS response from table 22-32 of the IPMI v2.0 spec
 */
struct user_access_rsp {
#if WORDS_BIGENDIAN
	uint8_t __reserved1 : 2;
	uint8_t maximum_ids : 6;
#else
	uint8_t maximum_ids : 6;
	uint8_t __reserved1 : 2;
#endif

#if WORDS_BIGENDIAN
	uint8_t __reserved2        : 2;
	uint8_t enabled_user_count : 6;
#else
	uint8_t enabled_user_count : 6;
	uint8_t __reserved2        : 2;
#endif

#if WORDS_BIGENDIAN
	uint8_t __reserved3      : 2;
	uint8_t fixed_name_count : 6;
#else
	uint8_t fixed_name_count : 6;
	uint8_t __reserved3      : 2;
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
#if WORDS_BIGENDIAN
	uint8_t __reserved4             : 1;
	uint8_t no_callin_access        : 1;
	uint8_t link_auth_access        : 1;
	uint8_t ipmi_messaging_access   : 1;
	uint8_t channel_privilege_limit : 4;
#else
	uint8_t channel_privilege_limit : 4;
	uint8_t ipmi_messaging_access   : 1;
	uint8_t link_auth_access        : 1;
	uint8_t no_callin_access        : 1;
	uint8_t __reserved4             : 1;
#endif
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

/* (22.27) Get and (22.26) Set User Access */
struct user_access_t {
	uint8_t callin_callback;
	uint8_t channel;
	uint8_t enabled_user_ids;
	uint8_t enable_status;
	uint8_t fixed_user_ids;
	uint8_t ipmi_messaging;
	uint8_t link_auth;
	uint8_t max_user_ids;
	uint8_t privilege_limit;
	uint8_t session_limit;
	uint8_t user_id;
};

/* (22.29) Get User Name */
struct user_name_t {
	uint8_t user_id;
	uint8_t user_name[17];
};

int ipmi_user_main(struct ipmi_intf *, int, char **);
int _ipmi_get_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_rsp);
int _ipmi_get_user_name(struct ipmi_intf *intf, struct user_name_t *user_name);
int _ipmi_set_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_req);

#endif /* IPMI_USER_H */