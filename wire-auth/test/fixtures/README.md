# Local TLS fixtures

These are synthetic localhost credentials generated with OpenSSL for transport
integration tests. The private key is intentionally public test data and must
never be used for deployment. The fixture library is built only when tests are
enabled and is not linked into the production executable.

The tests explicitly trust this certificate. Production HTTPS keeps certificate
verification enabled and uses system trust unless the caller supplies a private
CA bundle. No Mojang assets, credentials or network services are used.
