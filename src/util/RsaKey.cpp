#include "util/RsaKey.hpp"

#include <jwt-cpp/jwt.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

namespace fitplan::util {

namespace {

// unique_ptr aliases so every OpenSSL object frees itself on the way out - no
// leak on any of the throw paths below.
using BignumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using ParamBldPtr = std::unique_ptr<OSSL_PARAM_BLD, decltype(&OSSL_PARAM_BLD_free)>;
using ParamPtr = std::unique_ptr<OSSL_PARAM, decltype(&OSSL_PARAM_free)>;
using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

std::string base64url_decode(const std::string& in) {
    return jwt::base::decode<jwt::alphabet::base64url>(
        jwt::base::pad<jwt::alphabet::base64url>(in));
}

BignumPtr to_bignum(const std::string& big_endian_bytes) {
    const std::vector<unsigned char> bytes(big_endian_bytes.begin(), big_endian_bytes.end());
    BIGNUM* raw = BN_bin2bn(bytes.data(), static_cast<int>(bytes.size()), nullptr);
    return {raw, BN_free};
}

}  // namespace

std::string rsa_public_key_pem_from_jwk(const std::string& n_b64url, const std::string& e_b64url) {
    const BignumPtr n = to_bignum(base64url_decode(n_b64url));
    const BignumPtr e = to_bignum(base64url_decode(e_b64url));
    if (!n || !e) {
        throw std::runtime_error("JWK: modulus or exponent is not a valid integer");
    }

    // Collect the key's parameters (n, e) and turn them into an OSSL_PARAM array.
    const ParamBldPtr bld(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!bld || OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_RSA_N, n.get()) != 1 ||
        OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_RSA_E, e.get()) != 1) {
        throw std::runtime_error("JWK: could not assemble RSA key parameters");
    }
    const ParamPtr params(OSSL_PARAM_BLD_to_param(bld.get()), OSSL_PARAM_free);

    // Feed those parameters to the RSA key builder.
    const PkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr), EVP_PKEY_CTX_free);
    if (!ctx || !params || EVP_PKEY_fromdata_init(ctx.get()) != 1) {
        throw std::runtime_error("JWK: could not initialise the RSA key builder");
    }

    EVP_PKEY* pkey_raw = nullptr;
    if (EVP_PKEY_fromdata(ctx.get(), &pkey_raw, EVP_PKEY_PUBLIC_KEY, params.get()) != 1) {
        throw std::runtime_error("JWK: could not build the RSA public key");
    }
    const PkeyPtr pkey(pkey_raw, EVP_PKEY_free);

    // Serialise the key to a PEM string via an in-memory BIO.
    const BioPtr bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio || PEM_write_bio_PUBKEY(bio.get(), pkey.get()) != 1) {
        throw std::runtime_error("JWK: could not serialise the public key to PEM");
    }

    char* data = nullptr;
    const long length = BIO_get_mem_data(bio.get(), &data);
    return {data, static_cast<std::size_t>(length)};
}

}  // namespace fitplan::util
