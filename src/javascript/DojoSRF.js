if(!dojo._hasResource['DojoSRF']){

	dojo._hasResource['DojoSRF'] = true;
	dojo.provide('DojoSRF');

	// Note: this file was renamed from OpenSRF.js to DojoSRF.js,
	// but still provides resources with the OpenSRF namespace
	dojo.require('opensrf.opensrf', true);
	dojo.require('opensrf.opensrf_xhr', true);

	OpenSRF.session_cache = {};
	OpenSRF.CachedClientSession = function ( app ) {
		if (this.session_cache[app]) return this.session_cache[app];
		this.session_cache[app] = new OpenSRF.ClientSession ( app );
		return this.session_cache[app];
	}

	OpenSRF.locale = dojo.config.locale || (dojo.isIE ? navigator.userLanguage : navigator.language).toLowerCase();
}
