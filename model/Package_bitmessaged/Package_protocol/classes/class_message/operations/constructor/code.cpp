//~~ message(version versionObject) [message] ~~
unsigned char hash[SHA512_DIGEST_LENGTH];
SHA512(&(*myPayload)[0], myPayload.size(), hash);
memcpy(&checksum,hash,sizeof(checksum));