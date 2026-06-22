struct bctbx_ssl_config_struct {
	mbedtls_ssl_config *ssl_config;         /**< actual config structure */
	uint8_t ssl_config_externally_provided; /**< a flag, on when the ssl_config was provided by callers and not created
	                                           threw the new function */
	int (*callback_cli_cert_function)(void *,
	                                  bctbx_ssl_context_t *,
	                                  const bctbx_list_t *); /**< pointer to the callback called to update client
	              certificate during handshake callback params are user_data, ssl_context, list of server certificate
	              subject alt name and CN (null terminated strings) */
	void *callback_cli_cert_data;                            /**< data passed to the client cert callback */
#ifdef HAVE_DTLS_SRTP
	mbedtls_ssl_srtp_profile
	    dtls_srtp_mbedtls_profiles[MBEDTLS_TLS_SRTP_MAX_PROFILE_LIST_LENGTH +
	                               1]; /**< list of supported DTLS-SRTP profiles, mbedtls won't hold the reference, so
	                                      we must do it for the lifetime of the config structure. (size is +1 to add the
	                                      list termination) */
#endif                                 /* HAVE_DTLS_SRTP */
	int *ciphersuites;                 /**< ciphersuites as mbedtls id's */
	bctbx_ext_signing_key_ref_t *ext_key_ref;                           /**< an external key reference */
	mbedtls_svc_key_id_t ext_key_psa_id;                                /**< the psa ref to the external key */
	mbedtls_pk_context ext_key;                                         /**< the external key holder */
	bctbx_ssl_config_ext_sign_callback_t callback_ext_signing_function; /**< function to call for external signing */
	void *callback_ext_signing_data;
};
