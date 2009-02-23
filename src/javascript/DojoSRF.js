if(!dojo._hasResource['DojoSRF']){

	dojo._hasResource['DojoSRF'] = true;
	dojo.provide('DojoSRF');
	dojo.provide('OpenSRF');

	// Note: this file was renamed from OpenSRF.js to DojoSRF.js,
	// but still provides resources with the OpenSRF namespace
	dojo.require('opensrf.md5', true);
	dojo.require('opensrf.JSON_v1', true);
	dojo.require('opensrf.opensrf', true);
	dojo.require('opensrf.opensrf_xhr', true);

	OpenSRF.session_cache = {};
	OpenSRF.CachedClientSession = function ( app ) {
		if (this.session_cache[app]) return this.session_cache[app];
		this.session_cache[app] = new OpenSRF.ClientSession ( app );
		return this.session_cache[app];
	}

        localeRE = /^(\w\w)(-\w\w)?$/;
        localeMatch = localeRE.exec(dojo.locale);

        if (!localeMatch[1]) {
                OpenSRF.locale = dojo.isIE ? navigator.userLanguage : navigator.language;
        } else {
                OpenSRF.locale = localeMatch[1].toLowerCase();
        }
        if (localeMatch[2]) {
                OpenSRF.locale = OpenSRF.locale + localeMatch[2].toUpperCase();
        }
}
