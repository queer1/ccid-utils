/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <emv.h>

#include <stdio.h>

struct t_app {
	const uint8_t *aid;
	size_t aid_len;
	uint8_t asi;
};

static const struct t_app apps[] = {
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x03", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x29", .aid_len = 5, .asi = 1},
};

static int app_cmp(emv_app_t a, const struct t_app *b)
{
	if ( b->asi ) {
		emv_rid_t rid;
		emv_app_rid(a, rid);
		return memcmp(rid, b->aid, EMV_RID_LEN);
	}else{
		uint8_t aid[EMV_AID_LEN];
		size_t sz;
		emv_app_aid(a, aid, &sz);
		if ( sz < b->aid_len )
			return -1;
		if ( sz > b->aid_len )
			return 1;
		return memcmp(aid, b->aid, sz);
	}
}

static int app_supported(emv_app_t a)
{
	unsigned int i;

	for(i = 0; i < sizeof(apps)/sizeof(*apps); i++) {
		if ( !app_cmp(a, apps + i) )
			return 1;
	}

	return 0;
}

/* Step 0. Select application */
static int select_app(emv_t e)
{
	emv_app_t app;
	unsigned int i;

	/* Selection of apps by PSE directory */
	if ( emv_appsel_pse(e) ) {
		for(app = emv_appsel_pse_first(e); app;
			app = emv_appsel_pse_next(e, app)) {
			if ( !app_supported(app) ) {
				printf("emvtool: unsupported PSE app: %s\n",
					emv_app_pname(app));
				emv_app_delete(app);
				continue;
			}
			printf("emvtool: PSE app: %s\n",
				emv_app_pname(app));
		}

		for(app = emv_appsel_pse_first(e); app;
			app = emv_appsel_pse_next(e, app)) {
			if ( emv_app_select_pse(e, app) ) {
				printf("emvtool: Selected PSE app: %s\n",
					emv_app_pname(app));
				return 1;
			}
		}
	}

	/* Selection of apps by terminal list */
	for(i = 0; i < sizeof(apps)/sizeof(*apps); i++) {
		if ( !emv_app_select_aid(e, apps[i].aid, apps[i].aid_len) )
			continue;

		app = emv_current_app(e);
		if ( !app_cmp(app, apps + i) ) {
			printf("emvtool: Selected AID app: %s\n",
				emv_app_pname(app));
			return 1;
		}

		while ( emv_app_select_aid_next(e, apps[i].aid,
						apps[i].aid_len) ) {
			app = emv_current_app(e);
			if ( app_cmp(app, apps + i) )
				continue;
			printf("emvtool: Selected partial AID app: %s\n",
				emv_app_pname(app));
			return 1;
		}
	}

	return 0;
}

static int do_emv_stuff(chipcard_t cc)
{
	emv_t e;

	e = emv_init(cc);
	if ( NULL == e ) {
		fprintf(stderr, "emv: init error\n");
		return 0;
	}

	if ( !select_app(e) )
		goto end;

	/* Step 1. Initiate application processing */
	if ( !emv_app_init(e) )
		goto end;

	/* Step 2. Authenticate card */

	/* Step 3. Authenticate cardholder */

	/* Step 4. Authorize transaction */

end:	/* Step 5. terminate processing */
	emv_fini(e);
	return 1;
}

static int found_cci(ccidev_t dev)
{
	chipcard_t cc;
	cci_t cci;
	int ret = 0;

	cci = cci_probe(dev, "./emvtool.log");
	if ( NULL == cci )
		goto out;
	
	cc = cci_get_slot(cci, 0);
	if ( NULL == cc ) {
		fprintf(stderr, "ccid: error: no slots on CCI\n");
		goto out_close;
	}

	if ( !chipcard_wait_for_card(cc) )
		goto out_close;

	if ( !chipcard_slot_on(cc, CHIPCARD_AUTO_VOLTAGE, NULL) )
		goto out_close;

	if ( !do_emv_stuff(cc) )
		goto out_close;

	ret = 1;

	chipcard_slot_off(cc);
out_close:
	cci_close(cci);
out:
	return ret;
}

int main(int argc, char **argv)
{
	ccidev_t dev;

	dev = ccid_find_first_device();
	if ( NULL == dev )
		return EXIT_FAILURE;

	if ( !found_cci(dev) )
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
