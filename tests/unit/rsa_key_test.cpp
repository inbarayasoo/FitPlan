#include "util/RsaKey.hpp"

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>

#include <chrono>
#include <exception>
#include <string>

#include "RsaTestKey.hpp"

namespace {

using fitplan::testutil::make_rsa_test_key;
using fitplan::util::rsa_public_key_pem_from_jwk;

std::string sign_rs256(const std::string& private_pem) {
    return jwt::create()
        .set_issuer("https://accounts.google.com")
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes{5})
        .sign(jwt::algorithm::rs256("", private_pem));
}

TEST(RsaKeyTest, RebuildsAKeyThatVerifiesARealSignature) {
    const auto key = make_rsa_test_key("kid-1");

    const std::string pem = rsa_public_key_pem_from_jwk(key.n_b64url, key.e_b64url);
    const std::string token = sign_rs256(key.private_pem);

    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::rs256(pem))
                        .with_issuer("https://accounts.google.com");
    EXPECT_NO_THROW(verifier.verify(jwt::decode(token)));
}

TEST(RsaKeyTest, TheRebuiltKeyMatchesOpenSslsOwnPemForThatKey) {
    const auto key = make_rsa_test_key("kid-2");

    EXPECT_EQ(rsa_public_key_pem_from_jwk(key.n_b64url, key.e_b64url), key.public_pem);
}

TEST(RsaKeyTest, ARebuiltKeyRejectsATokenSignedByADifferentKey) {
    const auto key_a = make_rsa_test_key("a");
    const auto key_b = make_rsa_test_key("b");

    const std::string pem_a = rsa_public_key_pem_from_jwk(key_a.n_b64url, key_a.e_b64url);
    const std::string token_from_b = sign_rs256(key_b.private_pem);

    auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::rs256(pem_a));
    EXPECT_THROW(verifier.verify(jwt::decode(token_from_b)), std::exception);
}

TEST(RsaKeyTest, ThrowsWhenTheModulusIsNotBase64Url) {
    EXPECT_THROW(rsa_public_key_pem_from_jwk("has spaces and $", "AQAB"), std::exception);
}

}  // namespace
