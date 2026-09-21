#ifndef SECRETS_H
#define SECRETS_H

const char* WIFI_SSID = "VM0658551";
const char* WIFI_PASS = "acubyguPeWp3bbkf";

// Get these from developer.spotify.com
const char* SPOTIFY_CLIENT_ID = "3c0a083fb5234e47912e139c4111e375";
const char* SPOTIFY_CLIENT_SECRET = "891fbac6b7b24fed9be3a66f1c7b231c";

// You must generate this once using a web browser OAuth flow
const char* SPOTIFY_REFRESH_TOKEN = "AQBjFM_PoNqPOwgcM37O2VfwaMxz21WHCXXyyH2hQbfEGafgF8AKdcxk2L-GDBgnDyfj61Lz_17FhVYtK2XxnBFaWRnJy-q89Pxw3zA_KfiBYDD7gP-Gc5_do3HMxGrO57I";

#endif

// Redirect URI: 
// https://accounts.spotify.com/authorize?client_id=3c0a083fb5234e47912e139c4111e375&response_type=code&redirect_uri=https://example.com/callback&scope=user-library-read%20user-top-read%20user-modify-playback-state%20user-read-currently-playing
// https://example.com/callback?code=AQA3dwN1YBbwz56v39rTdtW_k2UXIvwfbjPaHmJEwBBoF0MAR13HzSam0lo6gyhvt20rnyA9rfdFr4K5-rkHQ683CVL4kGVg4K5-SpBsZeFE5QLdm8ZxxpkO4HzJvO2ALt0perr7j-hllXJFk0LDwLhiK8MIEQRHrdrQbp1ocGFgIzrPvP2rfpKQeQShFOSt-RCvU9gQ696dx22o4KTk7wja5odsJixqbI34vICumHCG3UucASf1KTxCuvWyqqSP7gMwYwi7u790coVhEPlsOSDfEZNYPosNd_UyEHeNMUIEpzaR_yZjTjH9oWNzpewyzQMhY28bXC6sKc_bRK-uG2js&ubi=CAIQ4a%2BA%2B4o0GiQyMDQzZDAzYS01MGUwLTRiNjQtYmU3ZS0xNmM5OTllYzNiMjgiJDI4NWIyMTJlLWNmNzEtNDBmYS04YWExLTI2ZDQ3MmM3YWNkMDokMjg1YjIxMmUtY2Y3MS00MGZhLThhYTEtMjZkNDcyYzdhY2QwQhB1c2VyX2ludGVyYWN0aW9uSiRhMTliNzYzMi1iY2VjLTQwMTAtOGU4NS0zZDliZmJlOWY3YTRQAA%3D%3D
// AQA3dwN1YBbwz56v39rTdtW_k2UXIvwfbjPaHmJEwBBoF0MAR13HzSam0lo6gyhvt20rnyA9rfdFr4K5-rkHQ683CVL4kGVg4K5-SpBsZeFE5QLdm8ZxxpkO4HzJvO2ALt0perr7j-hllXJFk0LDwLhiK8MIEQRHrdrQbp1ocGFgIzrPvP2rfpKQeQShFOSt-RCvU9gQ696dx22o4KTk7wja5odsJixqbI34vICumHCG3UucASf1KTxCuvWyqqSP7gMwYwi7u790coVhEPlsOSDfEZNYPosNd_UyEHeNMUIEpzaR_yZjTjH9oWNzpewyzQMhY28bXC6sKc_bRK-uG2js
/*
curl -X POST "https://accounts.spotify.com/api/token" \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "grant_type=authorization_code" \
     -d "code=AQA3dwN1YBbwz56v39rTdtW_k2UXIvwfbjPaHmJEwBBoF0MAR13HzSam0lo6gyhvt20rnyA9rfdFr4K5-rkHQ683CVL4kGVg4K5-SpBsZeFE5QLdm8ZxxpkO4HzJvO2ALt0perr7j-hllXJFk0LDwLhiK8MIEQRHrdrQbp1ocGFgIzrPvP2rfpKQeQShFOSt-RCvU9gQ696dx22o4KTk7wja5odsJixqbI34vICumHCG3UucASf1KTxCuvWyqqSP7gMwYwi7u790coVhEPlsOSDfEZNYPosNd_UyEHeNMUIEpzaR_yZjTjH9oWNzpewyzQMhY28bXC6sKc_bRK-uG2js" \
     -d "redirect_uri=https://example.com/callback" \
     -d "client_id=3c0a083fb5234e47912e139c4111e375" \
     -d "client_secret=891fbac6b7b24fed9be3a66f1c7b231c"
*/