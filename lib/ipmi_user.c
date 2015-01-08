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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_user.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/bswap.h>


extern int verbose;
extern int csv_output;


#define IPMI_PASSWORD_DISABLE_USER  0x00
#define IPMI_PASSWORD_ENABLE_USER   0x01
#define IPMI_PASSWORD_SET_PASSWORD  0x02
#define IPMI_PASSWORD_TEST_PASSWORD 0x03

/* _ipmi_get_user_access - Get User Access for given channel. Results are stored
 * into passed struct.
 *
 * @intf - IPMI interface
 * @user_access_rsp - ptr to user_access_t with UID and Channel set
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_get_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_rsp)
{
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	uint8_t data[2];
	if (user_access_rsp == NULL) {
		return (-3);
	}
	data[0] = user_access_rsp->channel & 0x0F;
	data[1] = user_access_rsp->user_id & 0x3F;
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_USER_ACCESS;
	req.msg.data = data;
	req.msg.data_len = 2;
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		return (-1);
	} else if (rsp->ccode != 0) {
		return rsp->ccode;
	} else if (rsp->data_len != 4) {
		return (-2);
	}
	user_access_rsp->max_user_ids = rsp->data[0] & 0x3F;
	user_access_rsp->enable_status = rsp->data[1] & 0xC0;
	user_access_rsp->enabled_user_ids = rsp->data[1] & 0x3F;
	user_access_rsp->fixed_user_ids = rsp->data[2] & 0x3F;
	user_access_rsp->callin_callback = rsp->data[3] & 0x40;
	user_access_rsp->link_auth = rsp->data[3] & 0x20;
	user_access_rsp->ipmi_messaging = rsp->data[3] & 0x10;
	user_access_rsp->privilege_limit = rsp->data[3] & 0x0F;
	return rsp->ccode;
}

/* _ipmi_get_user_name - Fetch User Name for given User ID. User Name is stored
 * into passed structure. 
 *
 * @intf - ipmi interface
 * @user_name - user_name_t struct with UID set
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_get_user_name(struct ipmi_intf *intf, struct user_name_t *user_name_ptr)
{
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	uint8_t data[1];
	if (user_name_ptr == NULL) {
		return (-3);
	}
	data[0] = user_name_ptr->user_id & 0x3F;
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_USER_NAME;
	req.msg.data = data;
	req.msg.data_len = 1;
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		return (-1);
	} else if (rsp->ccode > 0) {
		return rsp->ccode;
	} else if (rsp->data_len != 17) {
		return (-2);
	}
	memset(user_name_ptr->user_name, '\0', 17);
	memcpy(user_name_ptr->user_name, rsp->data, 16);
	return rsp->ccode;
}

/* _ipmi_set_user_access - Set User Access for given channel.
 *
 * @intf - IPMI interface
 * @user_access_req - ptr to user_access_t with desired User Access.
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_set_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_req)
{
	uint8_t data[4];
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	if (user_access_req == NULL) {
		return (-3);
	}
	data[0] = 0x80;
	if (user_access_req->callin_callback) {
		data[0] |= 0x40;
	}
	if (user_access_req->link_auth) {
		data[0] |= 0x20;
	}
	if (user_access_req->ipmi_messaging) {
		data[0] |= 0x10;
	}
	data[0] |= (user_access_req->channel & 0x0F);
	data[1] = user_access_req->user_id & 0x3F;
	data[2] = user_access_req->privilege_limit & 0x0F;
	data[3] = user_access_req->session_limit & 0x0F;
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_SET_USER_ACCESS;
	req.msg.data = data;
	req.msg.data_len = 4;
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		return (-1);
	} else { 
		return rsp->ccode;
	}
}

/*
 * ipmi_get_user_access
 *
 * param intf		[in]
 * param channel_number [in]
 * param user_id	[in]
 * param user_access	[out]
 *
 * return 0 on succes
 *	  1 on failure
 */
static int
ipmi_get_user_access(
		     struct ipmi_intf *intf,
		     uint8_t channel_number,
		     uint8_t user_id,
		     struct user_access_rsp *user_access)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	       msg_data[2];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	     /* 0x06 */
	req.msg.cmd	     = IPMI_GET_USER_ACCESS; /* 0x44 */
	req.msg.data     = msg_data;
	req.msg.data_len = 2;


	/* The channel number will remain constant throughout this function */
	msg_data[0] = channel_number;
	msg_data[1] = user_id;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get User Access command failed "
			"(channel %d, user %d)", channel_number, user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get User Access command failed "
			"(channel %d, user %d): %s", channel_number, user_id,
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(user_access,
	       rsp->data,
	       sizeof(struct user_access_rsp));

	return 0;
}



/*
 * ipmi_get_user_name
 *
 * param intf		[in]
 * param channel_number [in]
 * param user_id	[in]
 * param user_name	[out]
 *
 * return 0 on succes
 *	  1 on failure
 */
static int
ipmi_get_user_name(
		   struct ipmi_intf *intf,
		   uint8_t user_id,
		   char	*user_name)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	       msg_data[1];

	memset(user_name, 0, 17);

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;     /* 0x06 */
	req.msg.cmd      = IPMI_GET_USER_NAME; /* 0x45 */
	req.msg.data     = msg_data;
	req.msg.data_len = 1;

	msg_data[0] = user_id;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get User Name command failed (user %d)",
			user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		if (rsp->ccode == 0xcc)
			return 0;
		lprintf(LOG_ERR, "Get User Name command failed (user %d): %s",
			user_id, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(user_name, rsp->data, 16);

	return 0;
}




static void
dump_user_access(
		 uint8_t		  user_id,
		 const		   char * user_name,
		 struct user_access_rsp * user_access)
{
	static int printed_header = 0;

	if (! printed_header)
	{
		printf("ID  Name	     Callin  Link Auth	IPMI Msg   "
		       "Channel Priv Limit\n");
		printed_header = 1;
	}

	printf("%-4d%-17s%-8s%-11s%-11s%-s\n",
	       user_id,
	       user_name,
	       user_access->no_callin_access? "false": "true ",
	       user_access->link_auth_access? "true ": "false",
	       user_access->ipmi_messaging_access? "true ": "false",
	       val2str(user_access->channel_privilege_limit,
		       ipmi_privlvl_vals));
}



static void
dump_user_access_csv(
		     uint8_t user_id,
		     const char *user_name,
		     struct user_access_rsp *user_access)
{
	printf("%d,%s,%s,%s,%s,%s\n",
	       user_id,
	       user_name,
	       user_access->no_callin_access? "false": "true",
	       user_access->link_auth_access? "true": "false",
	       user_access->ipmi_messaging_access? "true": "false",
	       val2str(user_access->channel_privilege_limit,
		       ipmi_privlvl_vals));
}

static int
ipmi_print_user_list(
		     struct ipmi_intf *intf,
		     uint8_t channel_number)
{
	/* This is where you were! */
	char user_name[17];
	struct user_access_rsp  user_access;
	uint8_t current_user_id = 1;


	do
	{
		if (ipmi_get_user_access(intf,
					 channel_number,
					 current_user_id,
					 &user_access))
			return -1;


		if (ipmi_get_user_name(intf,
				       current_user_id,
				       user_name))
			return -1;

		if ((current_user_id == 0)	      ||
		    user_access.link_auth_access      ||
		    user_access.ipmi_messaging_access ||
		    strcmp("", user_name))
		{
			if (csv_output)
				dump_user_access_csv(current_user_id,
						     user_name, &user_access);
			else
				dump_user_access(current_user_id,
						 user_name,
						 &user_access);
		}


		++current_user_id;
	} while((current_user_id <= user_access.maximum_ids) &&
			(current_user_id <= IPMI_UID_MAX)); /* Absolute maximum allowed by spec */


	return 0;
}

/* ipmi_print_user_summary - print User statistics for given channel
 *
 * @intf - IPMI interface
 * @channel_number - channel number
 *
 * returns - 0 on success, (-1) on error
 */
static int
ipmi_print_user_summary(struct ipmi_intf *intf, uint8_t channel_number)
{
	struct user_access_t user_access = {0};
	int ccode = 0;
	user_access.channel = channel_number;
	user_access.user_id = 1;
	ccode = _ipmi_get_user_access(intf, &user_access);
	if (eval_ccode(ccode) != 0) {
		return (-1);
	}
	if (csv_output) {
		printf("%" PRIu8 ",%" PRIu8 ",%" PRIu8 "\n",
				user_access.max_user_ids,
				user_access.enabled_user_ids,
				user_access.fixed_user_ids);
	} else {
		printf("Maximum IDs	    : %" PRIu8 "\n",
				user_access.max_user_ids);
		printf("Enabled User Count  : %" PRIu8 "\n",
				user_access.enabled_user_ids);
		printf("Fixed Name Count    : %" PRIu8 "\n",
				user_access.fixed_user_ids);
	}
	return 0;
}

/*
 * ipmi_user_set_username
 */
static int
ipmi_user_set_username(
		       struct ipmi_intf *intf,
		       uint8_t user_id,
		       const char *name)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	       msg_data[17];

	/*
	 * Ensure there is space for the name in the request message buffer
	 */
	if (strlen(name) >= sizeof(msg_data)) {
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	     /* 0x06 */
	req.msg.cmd	     = IPMI_SET_USER_NAME;   /* 0x45 */
	req.msg.data     = msg_data;
	req.msg.data_len = sizeof(msg_data);
	memset(msg_data, 0, sizeof(msg_data));

	/* The channel number will remain constant throughout this function */
	msg_data[0] = user_id;
	strncpy((char *)(msg_data + 1), name, strlen(name));

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set User Name command failed (user %d, name %s)",
			user_id, name);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set User Name command failed (user %d, name %s): %s",
			user_id, name, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

static int
ipmi_user_set_userpriv(
		       struct ipmi_intf *intf,
		       uint8_t channel,
		       uint8_t user_id,
		       const unsigned char privLevel)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4] = {0, 0, 0, 0};

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;         /* 0x06 */
	req.msg.cmd      = IPMI_SET_USER_ACCESS;   /* 0x43 */
	req.msg.data     = msg_data;
	req.msg.data_len = 4;

	/* The channel number will remain constant throughout this function */
	msg_data[0] = (channel   & 0x0f);
	msg_data[1] = (user_id   & 0x3f);
	msg_data[2] = (privLevel & 0x0f);
	msg_data[3] = 0;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL)
	{
		lprintf(LOG_ERR, "Set Privilege Level command failed (user %d)",
			user_id);
		return -1;
	}
	if (rsp->ccode > 0)
	{
		lprintf(LOG_ERR, "Set Privilege Level command failed (user %d): %s",
			user_id, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

/*
 * ipmi_user_set_password
 *
 * This function is responsible for 4 things
 * Enabling/Disabling users
 * Setting/Testing passwords
 */
static int
ipmi_user_set_password(
		       struct ipmi_intf * intf,
		       uint8_t user_id,
		       uint8_t operation,
		       const char *password,
		       int is_twenty_byte_password)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	               msg_data[22];

	int password_length = (is_twenty_byte_password? 20 : 16);

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	    /* 0x06 */
	req.msg.cmd	 = IPMI_SET_USER_PASSWORD;  /* 0x47 */
	req.msg.data     = msg_data;
	req.msg.data_len = password_length + 2;


	/* The channel number will remain constant throughout this function */
	msg_data[0] = user_id;

	if (is_twenty_byte_password)
		msg_data[0] |= 0x80;

	msg_data[1] = operation;

	memset(msg_data + 2, 0, password_length);

	if (password != NULL)
		strncpy((char *)(msg_data + 2), password, password_length);

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set User Password command failed (user %d)",
			user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set User Password command failed (user %d): %s",
			user_id, val2str(rsp->ccode, completion_code_vals));
		return rsp->ccode;
	}

	return 0;
}



/*
 * ipmi_user_test_password
 *
 * Call ipmi_user_set_password, and interpret the result
 */
static int
ipmi_user_test_password(
			struct ipmi_intf * intf,
			uint8_t      user_id,
			const char       * password,
			int                is_twenty_byte_password)
{
	int ret;

	ret = ipmi_user_set_password(intf,
				     user_id,
				     IPMI_PASSWORD_TEST_PASSWORD,
				     password,
				     is_twenty_byte_password);

	switch (ret) {
	case 0:
		printf("Success\n");
		break;
	case 0x80:
		printf("Failure: password incorrect\n");
		break;
	case 0x81:
		printf("Failure: wrong password size\n");
		break;
	default:
		printf("Unknown error\n");
	}

	return ((ret == 0) ? 0 : -1);
}


/*
 * print_user_usage
 */
static void
print_user_usage(void)
{
	lprintf(LOG_NOTICE,
"User Commands:");
	lprintf(LOG_NOTICE,
"               summary      [<channel number>]");
	lprintf(LOG_NOTICE,
"               list         [<channel number>]");
	lprintf(LOG_NOTICE,
"               set name     <user id> <username>");
	lprintf(LOG_NOTICE,
"               set password <user id> [<password> <16|20>]");
	lprintf(LOG_NOTICE,
"               disable      <user id>");
	lprintf(LOG_NOTICE,
"               enable       <user id>");
	lprintf(LOG_NOTICE,
"               priv         <user id> <privilege level> [<channel number>]");
	lprintf(LOG_NOTICE,
"                     Privilege levels:");
	lprintf(LOG_NOTICE,
"                      * 0x1 - Callback");
	lprintf(LOG_NOTICE,
"                      * 0x2 - User");
	lprintf(LOG_NOTICE,
"                      * 0x3 - Operator");
	lprintf(LOG_NOTICE,
"                      * 0x4 - Administrator");
	lprintf(LOG_NOTICE,
"                      * 0x5 - OEM Proprietary");
	lprintf(LOG_NOTICE,
"                      * 0xF - No Access");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE,
"               test         <user id> <16|20> [<password]>");
	lprintf(LOG_NOTICE, "");
}


const char *
ipmi_user_build_password_prompt(uint8_t user_id)
{
	static char prompt[128];
	memset(prompt, 0, 128);
	snprintf(prompt, 128, "Password for user %d: ", user_id);
	return prompt;
}

/* ask_password - ask user for password
 *
 * @user_id: User ID which will be built-in into text
 *
 * @returns pointer to char with password
 */ 
char *
ask_password(uint8_t user_id)
{
	const char *password_prompt =
		ipmi_user_build_password_prompt(user_id);
# ifdef HAVE_GETPASSPHRASE
	return getpassphrase(password_prompt);
# else
	return (char*)getpass(password_prompt);
# endif
}

int
ipmi_user_summary(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Summary*/
	uint8_t channel;
	if (argc == 1) {
		channel = 0x0E; /* Ask about the current channel */
	} else if (argc == 2) {
		if (is_ipmi_channel_num(argv[1], &channel) != 0) {
			return (-1);
		}
	} else {
		print_user_usage();
		return (-1);
	}
	return ipmi_print_user_summary(intf, channel);
}

int
ipmi_user_list(struct ipmi_intf *intf, int argc, char **argv)
{
	/* List */
	uint8_t channel;
	if (argc == 1) {
		channel = 0x0E; /* Ask about the current channel */
	} else if (argc == 2) {
		if (is_ipmi_channel_num(argv[1], &channel) != 0) {
			return (-1);
		}
	} else {
		print_user_usage();
		return (-1);
	}
	return ipmi_print_user_list(intf, channel);
}

int
ipmi_user_test(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Test */
	char *password = NULL;
	int password_length = 0;
	uint8_t user_id = 0;
	/* a little irritating, isn't it */
	if (argc != 3 && argc != 4) {
		print_user_usage();
		return (-1);
	}
	if (is_ipmi_user_id(argv[1], &user_id)) {
		return (-1);
	}
	if (str2int(argv[2], &password_length) != 0
			|| (password_length != 16 && password_length != 20)) {
		lprintf(LOG_ERR,
				"Given password length '%s' is invalid.",
				argv[2]);
		lprintf(LOG_ERR, "Expected value is either 16 or 20.");
		return (-1);
	}
	if (argc == 3) {
		/* We need to prompt for a password */
		password = ask_password(user_id);
		if (password == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (-1);
		}
	} else {
		password = argv[3];
	}
	return ipmi_user_test_password(intf,
					 user_id,
					 password,
					 password_length == 20);
}

int
ipmi_user_priv(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t channel = 0x0e; /* Use channel running on */
	uint8_t priv_level = 0;
	uint8_t user_id = 0;

	if (argc != 3 && argc != 4) {
		print_user_usage();
		return (-1);
	}
	if (argc == 4) {
		if (is_ipmi_channel_num(argv[3], &channel) != 0) {
			return (-1);
		}
		channel = (channel & 0x0f);
	}
	if (is_ipmi_user_priv_limit(argv[2], &priv_level) != 0
			|| is_ipmi_user_id(argv[1], &user_id) != 0) {
		return (-1);
	}
	priv_level = (priv_level & 0x0f);
	return ipmi_user_set_userpriv(intf,channel,user_id,priv_level);
}

int
ipmi_user_mod(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Disable / Enable */
	uint8_t user_id;
	uint8_t operation;
	char null_password[16]; /* Not used, but required */

	if (argc != 2) {
		print_user_usage();
		return (-1);
	}
	if (is_ipmi_user_id(argv[1], &user_id)) {
		return (-1);
	}
	memset(null_password, 0, sizeof(null_password));
	operation = (strncmp(argv[0], "disable", 7) == 0) ?
		IPMI_PASSWORD_DISABLE_USER : IPMI_PASSWORD_ENABLE_USER;

	/* Last parameter is ignored */
	return ipmi_user_set_password(intf, user_id, operation, null_password, 0);
}

int
ipmi_user_password(struct ipmi_intf *intf, int argc, char **argv)
{
	char *password = NULL;
	uint8_t password_type = 16;
	uint8_t user_id = 0;
	if (is_ipmi_user_id(argv[2], &user_id)) {
		return (-1);
	}

	if (argc == 3) {
		/* We need to prompt for a password */
		char *tmp;
		password = ask_password(user_id);
		if (password == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (-1);
		}
		tmp = ask_password(user_id);
		if (tmp == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (-1);
		}
		if (strlen(password) != strlen(tmp)
				|| strncmp(password, tmp, strlen(tmp))) {
			lprintf(LOG_ERR, "Passwords do not match.");
			return (-1);
		}
	} else {
		password = argv[3];
		if (argc > 4) {
			if ((str2uchar(argv[4], &password_type) != 0)
					|| (password_type != 16 && password_type != 20)) {
				lprintf(LOG_ERR, "Invalid password length '%s'", argv[4]);
				return (-1);
			}
		} else {
			password_type = 16;
		}
	}

	if (password == NULL) {
		lprintf(LOG_ERR, "Unable to parse password argument.");
		return (-1);
	} else if (strlen(password) > 20) {
		lprintf(LOG_ERR, "Password is too long (> 20 bytes)");
		return (-1);
	}

	return ipmi_user_set_password(intf,
					user_id,
					IPMI_PASSWORD_SET_PASSWORD,
					password,
					password_type > 16);
}

int
ipmi_user_name(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Set Name */
	uint8_t user_id = 0;
	if (argc != 4) {
		print_user_usage();
		return (-1);
	}
	if (is_ipmi_user_id(argv[2], &user_id)) {
		return (-1);
	}
	if (strlen(argv[3]) > 16) {
		lprintf(LOG_ERR, "Username is too long (> 16 bytes)");
		return (-1);
	}

	return ipmi_user_set_username(intf, user_id, argv[3]);
}

/*
 * ipmi_user_main
 *
 * Upon entry to this function argv should contain our arguments
 * specific to this subcommand
 */
int
ipmi_user_main(struct ipmi_intf *intf, int argc, char **argv)
{
	if (argc == 0) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		print_user_usage();
		return (-1);
	}
	if (strncmp(argv[0], "help", 4) == 0) {
		/* Help */
		print_user_usage();
		return 0;
	} else if (strncmp(argv[0], "summary", 7) == 0) {
		return ipmi_user_summary(intf, argc, argv);
	} else if (strncmp(argv[0], "list", 4) == 0) {
		return ipmi_user_list(intf, argc, argv);
	} else if (strncmp(argv[0], "test", 4) == 0) {
		return ipmi_user_test(intf, argc, argv);
	} else if (strncmp(argv[0], "set", 3) == 0) {
		/* Set */
		if ((argc >= 3)
				&& (strncmp("password", argv[1], 8) == 0)) {
			return ipmi_user_password(intf, argc, argv);
		} else if ((argc >= 2)
				&& (strncmp("name", argv[1], 4) == 0)) {
			return ipmi_user_name(intf, argc, argv);
		} else {
			print_user_usage();
			return (-1);
		}
	} else if (strncmp(argv[0], "priv", 4) == 0) {
		return ipmi_user_priv(intf, argc, argv);
	} else if ((strncmp(argv[0], "disable", 7) == 0)
			|| (strncmp(argv[0], "enable",  6) == 0)) {
		return ipmi_user_mod(intf, argc, argv);
	} else {
		lprintf(LOG_ERR, "Invalid user command: '%s'\n", argv[0]);
		print_user_usage();
		return (-1);
	}
}