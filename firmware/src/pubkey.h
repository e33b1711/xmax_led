#pragma once
// Öffentlicher Schlüssel zum Prüfen der Update-Signatur (ECDSA P-256).
// Der private Teil liegt NUR auf dem Rechner, der Releases baut: ~/.config/xmax_led/signatur-privat.pem
static const char OTA_PUBKEY_PEM[] = R"KEY(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEoiN96GPoze8oxsEKfxTC+A2omqbj
ze55ONN1fvPgwC651yRjsegiEOHDf4DJXRdq4T41vbx6tov6z4cxI0EbZw==
-----END PUBLIC KEY-----
)KEY";
