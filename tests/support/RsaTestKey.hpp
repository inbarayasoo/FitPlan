#pragma once

// Test-only helper: generate a throwaway RSA keypair and expose it in every form
// the Step 9 code needs - PEM (private and public) and the JWK numbers n / e as
// base64url - plus helpers to assemble a fake Google JWKS document from one or
// more of them. Nothing here touches the network.

#include <jwt-cpp/jwt.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>

namespace fitplan::testutil {

inline std::string bio_to_string(BIO* bio) {
    char* data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    return std::string(data, static_cast<std::size_t>(len));
}

inline std::string base64url_no_pad(const std::string& raw) {
    return jwt::base::trim<jwt::alphabet::base64url>(
        jwt::base::encode<jwt::alphabet::base64url>(raw));
}

inline std::string bignum_to_base64url(const BIGNUM* bn) {
    std::string bytes(static_cast<std::size_t>(BN_num_bytes(bn)), '\0');
    BN_bn2bin(bn, reinterpret_cast<unsigned char*>(bytes.data()));
    return base64url_no_pad(bytes);
}

struct RsaTestKey {
    std::string kid;
    std::string private_pem;
    std::string public_pem;
    std::string n_b64url;
    std::string e_b64url;

    // One "keys" entry for this key, as Google's JWKS document formats it.
    std::string jwks_entry() const {
        return R"({"kty":"RSA","use":"sig","alg":"RS256","kid":")" + kid + R"(","n":")" + n_b64url +
               R"(","e":")" + e_b64url + R"("})";
    }

    // A full { "keys": [ ... ] } document holding exactly the given keys.
    static std::string jwks_document(std::initializer_list<RsaTestKey> keys) {
        std::string out = R"({"keys":[)";
        bool first = true;
        for (const RsaTestKey& k : keys) {
            if (!first) {
                out += ",";
            }
            out += k.jwks_entry();
            first = false;
        }
        return out + "]}";
    }
};

inline RsaTestKey make_rsa_test_key(const std::string& kid) {
    const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(EVP_RSA_gen(2048),
                                                                   EVP_PKEY_free);
    if (!pkey) {
        throw std::runtime_error("EVP_RSA_gen(2048) failed");
    }

    RsaTestKey key;
    key.kid = kid;

    {
        const std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), BIO_free);
        PEM_write_bio_PrivateKey(bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr);
        key.private_pem = bio_to_string(bio.get());
    }
    {
        const std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), BIO_free);
        PEM_write_bio_PUBKEY(bio.get(), pkey.get());
        key.public_pem = bio_to_string(bio.get());
    }
    {
        BIGNUM* n = nullptr;
        BIGNUM* e = nullptr;
        EVP_PKEY_get_bn_param(pkey.get(), OSSL_PKEY_PARAM_RSA_N, &n);
        EVP_PKEY_get_bn_param(pkey.get(), OSSL_PKEY_PARAM_RSA_E, &e);
        const std::unique_ptr<BIGNUM, decltype(&BN_free)> n_guard(n, BN_free);
        const std::unique_ptr<BIGNUM, decltype(&BN_free)> e_guard(e, BN_free);
        key.n_b64url = bignum_to_base64url(n);
        key.e_b64url = bignum_to_base64url(e);
    }
    return key;
}

}  // namespace fitplan::testutil
