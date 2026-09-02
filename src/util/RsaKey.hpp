#pragma once

#include <string>

namespace fitplan::util {

// Builds an RSA public key in PEM form from the two numbers a JWK carries: the
// modulus `n` and the public exponent `e`, each a base64url-encoded big-endian
// integer. That PEM is what jwt-cpp needs to verify an RS256 signature.
//
// Google's JWKS document (https://www.googleapis.com/oauth2/v3/certs) gives the
// keys this way - as { "kid", "kty":"RSA", "n", "e" } - with no ready-made PEM
// and no certificate to extract one from, so we assemble the key ourselves via
// libcrypto. Throws std::runtime_error if the inputs are not a valid RSA key.
std::string rsa_public_key_pem_from_jwk(const std::string& n_b64url, const std::string& e_b64url);

}  // namespace fitplan::util
