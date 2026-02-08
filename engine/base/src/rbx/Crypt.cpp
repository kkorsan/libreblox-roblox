// TODO: migrate to use newer cryptographic algorithms that are more secure than SHA1 and RSA

#include "rbx/Crypt.h"
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>

// Always include OpenSSL headers, as this is now a hard dependency.
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

namespace RBX
{
namespace
{
// --- Custom Deleters for OpenSSL types (for use with std::unique_ptr) ---
// This ensures that OpenSSL resources are always freed correctly (RAII).
struct BIODeleter {
    void operator()(BIO* bio) const { BIO_free_all(bio); }
};
struct RSADeleter {
    void operator()(RSA* rsa) const { RSA_free(rsa); }
};

using unique_bio_ptr = std::unique_ptr<BIO, BIODeleter>;
using unique_rsa_ptr = std::unique_ptr<RSA, RSADeleter>;


// --- Helper function to get OpenSSL error strings ---
std::string get_openssl_error()
{
    unique_bio_ptr bio(BIO_new(BIO_s_mem()));
    ERR_print_errors(bio.get());
    char* buffer = nullptr;
    long len = BIO_get_mem_data(bio.get(), &buffer);
    if (len > 0) {
        return std::string(buffer, len);
    }
    return "Unknown OpenSSL error";
}


// TODO(karma): Fetch from API?
const std::string rsaPublicKey =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjiCSWFga2esTPQoMGtEh\n"
    "p1dlKvI4RNFWVHC5fgL0xlEUAHD6L03/VZrraI8EQnQhnlUrcdpwK83hmRinc7Bd\n"
    "/WfQvP6Gb85CMS8FbvUbzZEzOWymMxgN+lM48lKpIygpfYdsDCQBlJ5wzKnUodkD\n"
    "MT4UmCrpYLz+lVsVDu0JKRKT6nRoOdwao6BoBL4CYyZbMGLxvmysb2s7B/zS3OxR\n"
    "UVfcqR5PvrJ6KCtbWMC63rUmp7O0xfATbNww4W94RJPy4EurEv+YfGrd9JSX2r8Y\n"
    "wb1bBsNamoqX3HQm3Q6kh+tDG45cmUGEHcd1kMyw+w+UbJpZGeNt+0ecYiu+p/YM\n"
    "SQIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

// --- Core Cryptographic Functions ---

/**
 * @brief Decodes a Base64 encoded string into a vector of bytes.
 * @param base64_input The Base64 encoded string.
 * @return A vector containing the decoded binary data.
 * @throws std::runtime_error on decoding failure.
 */
std::vector<unsigned char> unbase64(std::string base64_input)
{
    // Create a memory BIO from the input string.
    unique_bio_ptr b64_bio(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64_bio.get(), BIO_FLAGS_BASE64_NO_NL);

    unique_bio_ptr mem_bio(BIO_new_mem_buf(base64_input.data(), static_cast<int>(base64_input.length())));
    BIO_push(b64_bio.get(), mem_bio.release()); // b64_bio now owns mem_bio

    // Pre-allocate buffer, assuming decoded size is smaller than encoded.
    std::vector<unsigned char> output(base64_input.length());
    int decoded_length = BIO_read(b64_bio.get(), output.data(), static_cast<int>(output.size()));

    if (decoded_length < 0) {
        throw std::runtime_error("Base64 decode failed: " + get_openssl_error());
    }

    output.resize(decoded_length);
    return output;
}

/**
 * @brief Verifies a message against a Base64 encoded RSA-SHA1 signature.
 * @param message The plaintext message that was signed.
 * @param signature_b64 The Base64 encoded signature.
 * @return True if the signature is valid, false otherwise.
 */
bool verify_signature(std::string message, std::string signature_b64)
{
    // 1. Calculate the SHA1 hash of the message.
    std::vector<unsigned char> digest(SHA_DIGEST_LENGTH);
    SHA1(reinterpret_cast<const unsigned char*>(message.data()), message.length(), digest.data());

    // 2. Decode the Base64 signature.
    std::vector<unsigned char> signature_bin;
    try {
        signature_bin = unbase64(signature_b64);
    } catch (const std::runtime_error&) {
        // If decoding fails, the signature is invalid.
        return false;
    }

    // 3. Read the public key.
    unique_bio_ptr key_bio(BIO_new_mem_buf(rsaPublicKey.data(), static_cast<int>(rsaPublicKey.length())));
    if (!key_bio) {
        throw std::runtime_error("Failed to create BIO for public key: " + get_openssl_error());
    }

    unique_rsa_ptr rsa_key(PEM_read_bio_RSA_PUBKEY(key_bio.get(), NULL, NULL, NULL));
    if (!rsa_key) {
        throw std::runtime_error("Failed to read public key: " + get_openssl_error());
    }

    // 4. Verify the signature against the hash.
    int result = RSA_verify(
        NID_sha1,                  // Use SHA1
        digest.data(),             // The message hash
        digest.size(),             // Hash size
        signature_bin.data(),      // The decoded signature
        signature_bin.size(),      // Signature size
        rsa_key.get()              // The RSA public key
    );

    // RSA_verify returns 1 for success, 0 for failure, -1 for error.
    return result == 1;
}

} // anonymous namespace


// --- Public Class Implementation ---

Crypt::Crypt()
{
    // In a real-world application, you might call OpenSSL_add_all_algorithms() here,
    // but for SHA1 and RSA it's often not strictly necessary with modern OpenSSL.
}

Crypt::~Crypt()
{
}

void Crypt::verifySignatureBase64(std::string messageStr, std::string signatureStr)
{
    try
    {
        if (!verify_signature(messageStr, signatureStr)) {
            // Throw a generic error to prevent reverse engineering, as in the original.
            throw std::runtime_error("Signature verification failed.");
        }
    }
    catch (const std::runtime_error& e)
    {
        // For debugging, you might want to log e.what() here.
        // Re-throwing the specific error for clarity, or the generic one for security.
        throw std::runtime_error("Signature verification failed.");
    }
}

} // namespace RBX
