/* */

/*-
 * Copyright (c) 1997-2011 Roland C. Dowdeswell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer and
 *    dedication in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GSSAPI_GSSAPI_KRB5_H
#include <gssapi/gssapi_krb5.h>
#else
#ifdef HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#else
#ifdef HAVE_GSSAPI_H
#include <gssapi.h>
#else
#error Must have a gssapi header
#endif
#endif
#endif

#ifdef HAVE_KRB5_KRB5_H
#include <krb5/krb5.h>
#else
#ifdef HAVE_KRB5_H
#include <krb5.h>
#else
#error Must have krb5.h
#endif
#endif

#include "base64.h"

#define GBAIL(x, _maj, _min)	do {					\
		if (GSS_ERROR(_maj)) {					\
			char	*the_gss_err;				\
									\
			ret = 1;					\
			the_gss_err = gss_mk_err(_maj, _min, x);	\
			if (the_gss_err)				\
				fprintf(stderr, "%s\n", the_gss_err);	\
			else						\
				fprintf(stderr, "err making err\n");	\
			free(the_gss_err);				\
			goto bail;					\
		}							\
	} while (0)

#define K5BAIL(x)	do {						\
		kret = x;						\
		if (kret) {						\
			const char 	*k5err;				\
									\
			k5err = krb5_get_error_message(kctx, kret);	\
			if (k5err) {					\
				fprintf(stderr, "%s in %s:%s\n", k5err,	\
				    #x, __func__);			\
				krb5_free_error_message(kctx, k5err);	\
			} else {					\
				fprintf(stderr, "unknown error %d in "	\
				    "%s:%s\n", kret, #x, __func__);	\
			}						\
			exit(1); /* XXXrcd: shouldn't exit */		\
		}							\
	} while (0)


/*
 * global variables
 */

int	nflag = 0;

static char *
gss_mk_err(OM_uint32 maj_stat, OM_uint32 min_stat, const char *preamble)
{
	gss_buffer_desc	 status;
	OM_uint32	 new_stat;
	OM_uint32	 cur_stat;
	OM_uint32	 msg_ctx = 0;
	OM_uint32	 ret;
	int		 type;
	size_t		 newlen;
	char		*str = NULL;
	char		*tmp = NULL;

	cur_stat = maj_stat;
	type = GSS_C_GSS_CODE;

	for (;;) {

		/*
		 * GSS_S_FAILURE produces a rather unhelpful message, so
		 * we skip straight to the mech specific error in this case.
		 */

		if (type == GSS_C_GSS_CODE && cur_stat == GSS_S_FAILURE) {
			type = GSS_C_MECH_CODE;
			cur_stat = min_stat;
		}

		ret = gss_display_status(&new_stat, cur_stat, type,
		    GSS_C_NO_OID, &msg_ctx, &status);

		if (GSS_ERROR(ret))
			return str;	/* XXXrcd: hmmm, not quite?? */

		if (str)
			newlen = strlen(str);
		else
			newlen = strlen(preamble);

		newlen += status.length + 3;

		tmp = str;
		str = malloc(newlen);

		if (!str) {
			gss_release_buffer(&new_stat, &status);
			return tmp;	/* XXXrcd: hmmm, not quite?? */
		}

		snprintf(str, newlen, "%s%s%.*s", tmp?tmp:preamble,
		    tmp?", ":": ", (int)status.length, (char *)status.value);

		gss_release_buffer(&new_stat, &status);
		free(tmp);

		/*
		 * If we are finished processing for maj_stat, then
		 * move onto min_stat.
		 */

		if (msg_ctx == 0 && type == GSS_C_GSS_CODE && min_stat != 0) {
			type = GSS_C_MECH_CODE;
			cur_stat = min_stat;
			continue;
		}

		if (msg_ctx == 0)
			break;
	}

	return str;
}

static int
write_one_token(gss_name_t service, int delegate, int negotiate)
{
	gss_ctx_id_t	 ctx = GSS_C_NO_CONTEXT;
	gss_buffer_desc	 in;
	gss_buffer_desc	 out;
	OM_uint32	 maj;
	OM_uint32	 min;
	OM_uint32	 flags = 0;
	int		 ret = 0;
	char		*base64_output = NULL;

	in.length  = 0;
	in.value   = 0;
	out.length = 0;
	out.value  = 0;

	if (delegate)
		flags |= GSS_C_DELEG_FLAG;

        maj = gss_init_sec_context(&min, GSS_C_NO_CREDENTIAL, &ctx, service,
	    GSS_C_NO_OID, flags, 0, GSS_C_NO_CHANNEL_BINDINGS, &in, NULL, &out,
	    NULL, NULL);

	GBAIL("gss_init_sec_context", maj, min);

	base64_output = base64_encode(out.value, out.length);

	if (!base64_output) {
		fprintf(stderr, "Out of memory.\n");
		ret = 1;
		goto bail;
	}

	if (!nflag)
		printf("%s%s\n", negotiate?"Negotiate ":"", base64_output);

bail:
	if (out.value)
		gss_release_buffer(&min, &out);

	if (ctx != GSS_C_NO_CONTEXT) {
		/*
		 * XXXrcd: here we ignore the fact that we might have an
		 *         output token as this program doesn't do terribly
		 *         well in that case.
		 */
		gss_delete_sec_context(&min, &ctx, NULL);
	}

	free(base64_output);

	return ret;
}

static krb5_error_code
copy_cache(krb5_context kctx, krb5_ccache from, krb5_ccache to)
{
	krb5_error_code	kret;
	krb5_principal	princ = NULL;
	krb5_cc_cursor	cursor;
	krb5_creds	cred;

	K5BAIL(krb5_cc_get_principal(kctx, from, &princ));
	K5BAIL(krb5_cc_initialize(kctx, to, princ));
	K5BAIL(krb5_cc_start_seq_get(kctx, from, &cursor));
	for (;;) {
		kret = krb5_cc_next_cred(kctx, from, &cursor, &cred);
		if (kret)
			break;
		kret = krb5_cc_store_cred(kctx, to, &cred);
		krb5_free_cred_contents(kctx, &cred);
		if (kret)
			break;
	}
	krb5_cc_end_seq_get(kctx, from, &cursor);

	if (kret == KRB5_CC_END)
		kret = 0;
	K5BAIL(kret);

bail:
	if (princ)
		krb5_free_principal(kctx, princ);

	return kret;
}

static int
write_token(gss_name_t service, int delegate, int negotiate, int memcache,
	    size_t count)
{
	krb5_error_code	kret;
	krb5_context	kctx = NULL;
	krb5_ccache	def_cache = NULL;
	krb5_ccache	mem_cache = NULL;
	size_t		i;

	if (memcache) {
		K5BAIL(krb5_init_context(&kctx));
		K5BAIL(krb5_cc_default(kctx, &def_cache));
		K5BAIL(krb5_cc_resolve(kctx, "MEMORY:mem_cache", &mem_cache));
		putenv("KRB5CCNAME=MEMORY:mem_cache");
	}

	for (i=0; i < count; i++) {
		if (memcache)
			K5BAIL(copy_cache(kctx, def_cache, mem_cache));
		kret = write_one_token(service, delegate, negotiate);

		if (!nflag && i < count - 1)
			printf("\n");
	}

bail:
	if (kctx)
		krb5_free_context(kctx);
	if (def_cache)
		krb5_cc_close(kctx, def_cache);
	if (mem_cache)
		krb5_cc_close(kctx, mem_cache);

	return kret;
}

static char *
read_buffer(FILE *fp)
{
	char	 buf[65536];
	char	*p;
	char	*ret = NULL;
	size_t	 buflen;
	size_t	 retlen = 0;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if ((p = strchr(buf, '\n')) == NULL) {
			fprintf(stderr, "Long line, exiting.\n");
			exit(1);
		}

		*p = '\0';

		buflen = strlen(buf);

		if (buflen == 0)
			break;

		ret = realloc(ret, retlen + buflen + 1);

		memcpy(ret + retlen, buf, buflen);
		ret[retlen + buflen] = '\0';
	}

	if (ferror(stdin)) {
		perror("fgets");
		exit(1);
	}

	return ret;
}

static int
read_one_token(gss_name_t service, const char *ccname, int negotiate)
{
	gss_cred_id_t	 cred = NULL;
	gss_cred_id_t	 deleg_creds = NULL;
        gss_name_t       client;
        gss_OID          mech_oid;
        gss_ctx_id_t     ctx = GSS_C_NO_CONTEXT;
        gss_buffer_desc  in, out, dname;
	krb5_context	 kctx = NULL;
	krb5_ccache	 ccache = NULL;
	krb5_error_code	 kret;
        OM_uint32        maj, min;
	char		*inbuf = NULL;
	char		*tmp;
	char		 buf[65536];
	int		 ret = 0;

	if (service) {
		maj = gss_acquire_cred(&min, service, 0, NULL, GSS_C_ACCEPT,
		    &cred, NULL, NULL);
		GBAIL("gss_acquire_cred", maj, min);
	}

	inbuf = read_buffer(stdin);
	if (!inbuf)
		/* Just a couple of \n's in a row or EOF, not an error. */
		return 0;

	tmp = inbuf;
	if (negotiate) {
		if (strncasecmp("Negotiate ", inbuf, 10)) {
			fprintf(stderr, "Token doesn't begin with "
			    "\"Negotiate \"\n");
			return -1;
		}

		tmp += 10;
	}

	in.length = base64_decode((uint8_t *)tmp, strlen(tmp),
	    (uint8_t *)buf, sizeof(buf));
	in.value  = buf;

	out.length = 0;
	out.value  = 0;
 
        maj = gss_accept_sec_context(&min, &ctx, cred, &in,
	    GSS_C_NO_CHANNEL_BINDINGS, &client, &mech_oid, &out,
	    NULL, NULL, &deleg_creds);

	GBAIL("gss_accept_sec_context", maj, min);

	/*
	 * XXXrcd: not bothering to clean up because we're about to exit.
	 *         Probably should fix this in case the code is used as
	 *         an example by someone.
	 */

	maj = gss_display_name(&min, client, &dname, NULL);
	GBAIL("gss_display_name", maj, min);

	if (!nflag)
		printf("Authenticated: %.*s\n", (int)dname.length,
		    (char *)dname.value);

	if (ccname) {
#ifdef HAVE_GSS_STORE_CRED_INTO
		gss_key_value_set_desc		store;
		gss_key_value_element_desc	elem;
		int				overwrite_cred = 1;
		int				default_cred = 0;

		elem.key = "ccache";
		elem.value = ccname;
		store.count = 1;
		store.elements = &elem;

		maj = gss_store_cred_into(&min, deleg_creds, GSS_C_INITIATE,
		    GSS_C_NO_OID, overwrite_cred, default_cred, &store, NULL,
		    NULL);
		GBAIL("gss_store_cred_into", maj, min);
#else
		K5BAIL(krb5_init_context(&kctx));
		K5BAIL(krb5_cc_resolve(kctx, ccname, &ccache));

		maj = gss_krb5_copy_ccache(&min, deleg_creds, ccache);
		GBAIL("gss_krb5_copy_ccache", maj, min);
#endif
	}

bail:
	if (kctx)
		krb5_free_context(kctx);
	if (ccache)
		krb5_cc_close(kctx, ccache);
	if (cred)
		gss_release_cred(&min, &cred);
	if (deleg_creds)
		gss_release_cred(&min, &deleg_creds);

	free(inbuf);

	return ret;
}

static int
read_token(gss_name_t service, const char *ccname, int negotiate, size_t count)
{
	size_t	i;
	int	ret;

	for (i=0; i < count; i++) {
		ret = read_one_token(service, ccname, negotiate);
	}

	return ret;
}

static gss_name_t
import_service(char *service)
{
	gss_buffer_desc	name;
	gss_name_t	svc = NULL;
	OM_uint32	maj;
	OM_uint32	min;
	int		ret = 0;

	name.length = strlen(service);
	name.value  = service;

	maj = gss_import_name(&min, &name, GSS_C_NT_HOSTBASED_SERVICE, &svc);

	GBAIL("gss_import_name", maj, min);

bail:
	if (ret)
		exit(1);
	return svc;
}

static void
usage(void)
{

	fprintf(stderr, "usage: gss-token [-DNn] [-c count] "
	    "service@host\n");
	fprintf(stderr, "       gss-token -r [-Nln] [-C ccache] [-c count] "
	    "[service@host]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	OM_uint32	 min;
	gss_name_t	 service = NULL;
	extern char	*optarg;
	extern int	 optind;
	size_t		 count = 1;
	int		 ch;
	int		 Dflag = 0;
	int		 Mflag = 0;
	int		 Nflag = 0;
	int		 lflag = 0;
	int		 rflag = 0;
	int		 ret = 0;
	char		*ccname = NULL;

	while ((ch = getopt(argc, argv, "C:DMNc:nlr")) != -1) {
		switch (ch) {
		case 'C':
			ccname = optarg;
			break;
		case 'D':
			Dflag = 1;
			break;
		case 'M':
			Mflag = 1;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		service = import_service(*argv);

	if (!rflag) {
		if (!argc) {
			fprintf(stderr, "Without -r, hostbased_service must "
			    "be provided.\n");
			usage();
		}
		if (ccname) {
			fprintf(stderr, "Specifying a target ccache doesn't "
			    "make sense without -r.\n");
			usage();
		}
		ret = write_token(service, Dflag, Nflag, Mflag, count);
		goto done;
	}

	if (Dflag) {
		fprintf(stderr, "Delegating credentials (-D) doesn't make "
		    "sense when reading tokens (-r).\n");
		usage();
	}

	do {
		ret = read_token(service, ccname, Nflag, count);
	} while (lflag && !ret && !feof(stdin));

done:
	if (service)
		gss_release_name(&min, &service);

	return ret;
}
