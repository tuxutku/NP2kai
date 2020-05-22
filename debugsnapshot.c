/* === NP2 debug snapshot === (c) 2020 AZO */

#include <debugsnapshot.h>
#include <dosio.h>
#include <fdd/sxsi.h>
#include <fdd/sxsicd.h>
#include <vram/scrnsave.h>
#include <np2ver.h>
#include <statsave.h>

#if defined(__LIBRETRO__)
#include <rhash.h>
#elif(_MSC_VER)
#include <windows.h>
#else
#include <openssl/sha.h>
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#define DBSS_MSG(string) _RPTN(_CRT_WARN, "%s\n", string)
#else
#define DBSS_MSG(string) printf("%s\n", string)
#endif

UINT NP2_DebugSnapshot_Count;

NP2_DebugSnapshot_t tDebugSnapshot;

#if defined(_MSC_VER)
struct CALCSHA1 {
  HCRYPTPROV hProv;
  HCRYPTHASH hHash;
};
#endif

HCALCSHA1 calc_sha1_begin(void) {
  HCALCSHA1 hRes;

#if defined(__LIBRETRO__)
  hRes = (HCALCSHA1)malloc(sizeof(struct sha1_context));
  if(hRes) {
    SHA1Reset(hRes);
  }
#elif defined(_MSC_VER)
  struct CALCSHA1 {
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;
  };

  hRes = (HCALCSHA1)malloc(sizeof(struct CALCSHA1));
  if(hRes) {
    if(!CryptAcquireContext(&((struct CALCSHA1*)hRes)->hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
      CryptReleaseContext(((struct CALCSHA1*)hRes)->hProv, 0);
      free(hRes);
      hRes = NULL;
    }
  }
  if(hRes) {
    if(!CryptCreateHash(((struct CALCSHA1*)hRes)->hProv, CALG_SHA1, 0, 0, &((struct CALCSHA1*)hRes)->hHash)) {
      CryptDestroyHash(((struct CALCSHA1*)hRes)->hHash);
      CryptReleaseContext(((struct CALCSHA1*)hRes)->hProv, 0);
      free(hRes);
      hRes = NULL;
    }
  }
#else
  hRes = (HCALCSHA1)malloc(sizeof(SHA_CTX));
  if(hRes) {
    if(SHA1_Init((SHA_CTX*)hRes) != 1) {
      free(hRes);
      hRes = NULL;
    }
  }
#endif

  return hRes;
}

void calc_sha1_destruct(HCALCSHA1 hCalc) {
  if(hCalc) {
#if defined(_MSC_VER)
    CryptDestroyHash(((struct CALCSHA1*)hCalc)->hHash);
    CryptReleaseContext(((struct CALCSHA1*)hCalc)->hProv, 0);
#endif
    free(hCalc);
  }
}

UINT calc_sha1_add(HCALCSHA1 hCalc, const UINT8* pu8Data, const UINT uLen) {
  UINT uRes = 0;

  if(!hCalc || !pu8Data) {
    uRes = 1;
  }
  if(!uRes && uLen > 0) {
#if defined(__LIBRETRO__)
    SHA1Input((struct sha1_context*)hCalc, pu8Data, uLen);
#elif defined(_MSC_VER)
    if(!CryptHashData(((struct CALCSHA1*)hCalc)->hHash, pu8Data, uLen, 0)) {
      uRes = 2;
      CryptDestroyHash(((struct CALCSHA1*)hCalc)->hHash);
      CryptReleaseContext(((struct CALCSHA1*)hCalc)->hProv, 0);
      free(hCalc);
    }
#else
    if(SHA1_Update((SHA_CTX*)hCalc, pu8Data, uLen) != 1) {
      uRes = 2;
      free(hCalc);
    }
#endif
  }

  return uRes;
}

UINT calc_sha1_end(HCALCSHA1 hCalc, UINT8 au8SHA1[20]) {
  UINT uRes = 0;

  if(!hCalc || !au8SHA1) {
    uRes = 1;
  }
  if(!uRes) {
#if defined(__LIBRETRO__)
    if(!SHA1Result((struct sha1_context*)hCalc)) {
      uRes = 2;
      free(hCalc);
    }
  }
  if(!uRes) {
    int i;
    for(i = 0; i < 20; i++) {
      au8SHA1[i] = ((UINT8*)(((struct sha1_context*)hCalc)->Message_Digest))[(i / 4) * 4 + (3 - (i % 4))];
    }
#elif defined(_MSC_VER)
    UINT uLen = 20;
    if(!CryptGetHashParam(((struct CALCSHA1*)hCalc)->hHash, HP_HASHVAL, au8SHA1, &uLen, 0)) {
      uRes = 2;
      CryptDestroyHash(((struct CALCSHA1*)hCalc)->hHash);
      CryptReleaseContext(((struct CALCSHA1*)hCalc)->hProv, 0);
      free(hCalc);
    }
  }
  if(!uRes) {
    CryptDestroyHash(((struct CALCSHA1*)hCalc)->hHash);
    CryptReleaseContext(((struct CALCSHA1*)hCalc)->hProv, 0);
#else
    if(SHA1_Final(au8SHA1, (SHA_CTX*)hCalc) != 1) {
      uRes = 2;
      free(hCalc);
    }
#endif
  }
  if(!uRes) {
    free(hCalc);
  }

  return uRes;
}

#define CALC_HASH_BUFFERSIZE 0x100000

UINT calc_sha1(UINT8 au8SHA1[20], const OEMCHAR* strFilePath) {
  UINT uRes = 0;
  FILEH f;
  UINT8* pu8Buf;
  UINT uLen;
  HCALCSHA1 hCalc;

  if(!au8SHA1 || !strFilePath) {
    uRes = 1;
  }
  if(!uRes) {
    if(strFilePath[0] == '\0') {
      uRes = 2;
    }
  }
  if(!uRes) {
    hCalc = calc_sha1_begin();
    if(!hCalc) {
      uRes = 3;
    }
  }
  if(!uRes) {
    f = file_open_rb(strFilePath);
    if(!f) {
      uRes = 4;
      calc_sha1_destruct(hCalc);
    }
  }
  if(!uRes) {
    pu8Buf = (UINT8*)malloc(CALC_HASH_BUFFERSIZE);
    if(!pu8Buf) {
      uRes = 5;
      calc_sha1_destruct(hCalc);
      file_close(f);
    }
  }
  if(!uRes) {
    while(uLen = file_read(f, pu8Buf, CALC_HASH_BUFFERSIZE)) {
      if(calc_sha1_add(hCalc, pu8Buf, uLen)) {
        uRes = 6;
        calc_sha1_destruct(hCalc);
        file_close(f);
        break;
      }
    }
  }
  if(!uRes) {
    if(calc_sha1_end(hCalc, au8SHA1)) {
      uRes = 7;
      free(pu8Buf);
      file_close(f);
    }
  }
  if(!uRes) {
    free(pu8Buf);
    file_close(f);
  }

  return uRes;
}

int debugsnapshot_save(const UINT uNo) {
  UINT uRes = 0;
  INT iVal;
  OEMCHAR* pstrBaseDir;
  OEMCHAR strFilePath[MAX_PATH];
  SCRNSAVE hScreenSave;
  FILEH hFile;
  short attr;

  memset(&tDebugSnapshot, 0, sizeof(NP2_DebugSnapshot_t));

#if defined(__LIBRETRO__)
  milstr_ncpy(tDebugSnapshot.strProgramType, "libretro", sizeof(tDebugSnapshot.strProgramType));
#elif defined(EMSCRIPTEN)
#if defined(SUPPORT_PC9821)
  milstr_ncpy(tDebugSnapshot.strProgramType, "Emscripten IA-32", sizeof(tDebugSnapshot.strProgramType));
#else
  milstr_ncpy(tDebugSnapshot.strProgramType, "Emscripten", sizeof(tDebugSnapshot.strProgramType));
#endif
#elif defined(NP2_SDL)
#if defined(SUPPORT_PC9821)
  milstr_ncpy(tDebugSnapshot.strProgramType, "SDL IA-32", sizeof(tDebugSnapshot.strProgramType));
#else
  milstr_ncpy(tDebugSnapshot.strProgramType, "SDL", sizeof(tDebugSnapshot.strProgramType));
#endif
#elif defined(NP2_X)
#if defined(SUPPORT_PC9821)
  milstr_ncpy(tDebugSnapshot.strProgramType, "X IA-32", sizeof(tDebugSnapshot.strProgramType));
#else
  milstr_ncpy(tDebugSnapshot.strProgramType, "X", sizeof(tDebugSnapshot.strProgramType));
#endif
#elif defined(NP2_WIN)
#if defined(SUPPORT_PC9821)
  milstr_ncpy(tDebugSnapshot.strProgramType, "Windows IA-32", sizeof(tDebugSnapshot.strProgramType));
#else
  milstr_ncpy(tDebugSnapshot.strProgramType, "Windows", sizeof(tDebugSnapshot.strProgramType));
#endif
#else
  milstr_ncpy(tDebugSnapshot.strProgramType, "(unknown)", sizeof(tDebugSnapshot.strProgramType));
#endif

  pstrBaseDir = file_getcd(OEMTEXT(DEBUGSS_DIRNAME));
  attr = file_attr(pstrBaseDir);
  if(attr >= 0) {
    if(attr & ~FILEATTR_DIRECTORY) {
      DBSS_MSG("[debugss] debugss is not valid directory.");
      uRes = 1;
    }
  } else {
    if(file_dircreate(pstrBaseDir) != 0) {
      DBSS_MSG("[debugss] could not create debugss directory.");
      uRes = 2;
    }
  }
  if(!uRes) {
    attr = file_attr(pstrBaseDir);
    if(attr >= 0) {
      if(attr & FILEATTR_READONLY) {
        DBSS_MSG("[debugss] could not create debugss file (read only).");
        uRes = 3;
      }
    } else {
      if(attr & FILEATTR_READONLY) {
        DBSS_MSG("[debugss] could not create debugss file.");
        uRes = 4;
      }
    }
  }

  if(!uRes) {
    OEMSTRCPY(tDebugSnapshot.strVersion, OEMTEXT(NP2VER_CORE) OEMTEXT(" ") OEMTEXT(NP2VER_GIT));
    OEMSNPRINTF(
      tDebugSnapshot.strStatePath, MAX_PATH, OEMTEXT("%s%c%s_%d.state"),
      pstrBaseDir,
      OEMPATHDIVC,
      DEBUGSS_FILENAME,
      uNo
    );
    statsave_save(tDebugSnapshot.strStatePath);

    hScreenSave = scrnsave_create();
    OEMSNPRINTF(
      tDebugSnapshot.strBMPPath, MAX_PATH, OEMTEXT("%s%c%s_%d.bmp"),
      pstrBaseDir,
      OEMPATHDIVC,
      DEBUGSS_FILENAME,
      uNo
    );
    scrnsave_writebmp(hScreenSave, tDebugSnapshot.strBMPPath, 0);
    scrnsave_destroy(hScreenSave);

    for(iVal = 0; iVal < MAX_FDDFILE; iVal++) {
      if(fddfile[iVal].fname[0] != '\0') {
        tDebugSnapshot.uFDMount |= (1 << iVal);
        tDebugSnapshot.auFDType[iVal] = fddfile[iVal].ftype;
        file_cpyname(tDebugSnapshot.astrFDImagePath[iVal], fddfile[iVal].fname, MAX_PATH);
        tDebugSnapshot.auFDRO[iVal] = fddfile[iVal].ro;
        memcpy(&tDebugSnapshot.auFDInfo[iVal], &fddfile[iVal].inf, sizeof(union fdinfo));
        memcpy(tDebugSnapshot.au8FDHash[iVal], fddfile[iVal].hash_sha1, 20);
        calc_sha1(tDebugSnapshot.au8FDHash[iVal], fddfile[iVal].fname);
      }
    }

    for(iVal = 0; iVal < SASIHDD_MAX + SCSIHDD_MAX; iVal++) {
      if(sxsi_dev[iVal].fname[0] != '\0') {
        tDebugSnapshot.uHDCDMount |= (1 << iVal);
        tDebugSnapshot.auHDCDType[iVal] = sxsi_dev[iVal].devtype;
        file_cpyname(tDebugSnapshot.astrHDCDImagePath[iVal], sxsi_dev[iVal].fname, MAX_PATH);
        if(sxsi_dev[iVal].devtype == SXSIDEV_CDROM && (milstr_cmp(sxsi_dev[iVal].fname, OEMTEXT("\\\\.\\")) != 0)) {
          if(np2cfg.debugss) {
            HCALCSHA1 hCalc;
            UINT8 au8Buf[2352];
            UINT uTracks, uSectors, uSectorNo, uSize;
            CDTRK atTracks = sxsicd_gettrk(&sxsi_dev[iVal], &uTracks);
            if(atTracks && uTracks) {
              hCalc = calc_sha1_begin();
              if(hCalc) {
                uSectors = atTracks[uTracks - 1].pos + atTracks[uTracks - 1].sectors;
//printf("CD Sectors: %d\n", uSectors);
                for(uSectorNo = 0; uSectorNo < uSectors; uSectorNo++) {
                  if(sxsicd_readraw_forhash(&sxsi_dev[iVal], uSectorNo, au8Buf, &uSize) == 0) {
                    calc_sha1_add(hCalc, au8Buf, uSize);
                  } else {
//printf("Error sector: %d\n", uSectorNo);
                  }
                }
              }
              calc_sha1_end(hCalc, tDebugSnapshot.au8CDHash[iVal]);
//printf("Hash: ");
//              for(uSectorNo = 0; uSectorNo < 20; uSectorNo++) {
//printf("%02x", tDebugSnapshot.au8CDHash[iVal][uSectorNo]);
//              }
//printf("\n");
            }
          } else {
            memset(tDebugSnapshot.au8CDHash[iVal], 0, 20);
          }
        }
      }
    }
  }

  if(!uRes) {
    memcpy(&tDebugSnapshot.np2cfg, &np2cfg, sizeof(NP2CFG));
    memcpy(&tDebugSnapshot.np2oscfg, &np2oscfg, sizeof(NP2OSCFG));
  }

  if(!uRes) {
    OEMSNPRINTF(
      strFilePath, MAX_PATH, OEMTEXT("%s%c%s_%d.data"),
      pstrBaseDir,
      OEMPATHDIVC,
      DEBUGSS_FILENAME,
      uNo
    );
    hFile = NULL;
    hFile = file_create(strFilePath);
    if(!hFile) {
      DBSS_MSG("[debugss] could not create debug data file.");
      uRes = 10;
    }
  }
  if(!uRes) {
    file_write(hFile, &tDebugSnapshot, sizeof(NP2_DebugSnapshot_t));
    file_close(hFile);
    hFile = NULL;
  }

  return uRes;
}

int debugsnapshot_load(const UINT uNo) {
  UINT uRes = 0;
  INT iVal, iVal2;
  OEMCHAR* pstrBaseDir;
  OEMCHAR strFilePath[MAX_PATH];
  OEMCHAR strString[MAX_PATH];
  FILEH hFile;

  memset(&tDebugSnapshot, 0, sizeof(NP2_DebugSnapshot_t));

  pstrBaseDir = file_getcd(OEMTEXT(DEBUGSS_DIRNAME));
  iVal = file_attr(pstrBaseDir);
  if(iVal == 0) {
    DBSS_MSG("[debugss] not found debug directory.");
    uRes = 1;
  }

  if(!uRes) {
    OEMSNPRINTF(
      strFilePath, MAX_PATH, OEMTEXT("%s%c%s_%d.data"),
      pstrBaseDir,
      OEMPATHDIVC,
      DEBUGSS_FILENAME,
      uNo
    );
    hFile = NULL;
    hFile = file_open_rb(strFilePath);
    if(!hFile) {
      DBSS_MSG("[debugss] could not open debug data file.");
      uRes = 3;
    } else {
      file_read(hFile, &tDebugSnapshot, sizeof(NP2_DebugSnapshot_t));
      file_close(hFile);
      hFile = NULL;
    }
  }

  if(!uRes) {
#if defined(__LIBRETRO__)
    if(milstr_cmp(tDebugSnapshot.strProgramType, "libretro") != 0) {
#elif defined(EMSCRIPTEN)
#if defined(SUPPORT_PC9821)
    if(milstr_cmp(tDebugSnapshot.strProgramType, "Emscripten IA-32") != 0) {
#else
    if(milstr_cmp(tDebugSnapshot.strProgramType, "Emscripten") != 0) {
#endif
#elif defined(NP2_SDL)
#if defined(SUPPORT_PC9821)
    if(milstr_cmp(tDebugSnapshot.strProgramType, "SDL IA-32") != 0) {
#else
    if(milstr_cmp(tDebugSnapshot.strProgramType, "SDL") != 0) {
#endif
#elif defined(NP2_X)
#if defined(SUPPORT_PC9821)
    if(milstr_cmp(tDebugSnapshot.strProgramType, "X IA-32") != 0) {
#else
    if(milstr_cmp(tDebugSnapshot.strProgramType, "X") != 0) {
#endif
#elif defined(NP2_WIN)
#if defined(SUPPORT_PC9821)
    if(milstr_cmp(tDebugSnapshot.strProgramType, "Windwos IA-32") != 0) {
#else
    if(milstr_cmp(tDebugSnapshot.strProgramType, "Windwos") != 0) {
#endif
#else
    if(milstr_cmp(tDebugSnapshot.strProgramType, "(unknown)") == 0) {
#endif
      OEMSNPRINTF(
        strString, MAX_PATH, OEMTEXT("[debugss] not match program type. : %s"),
        tDebugSnapshot.strProgramType
      );
      DBSS_MSG(strString);
      uRes = 3;
    } else {
      OEMSNPRINTF(
        strString, MAX_PATH, OEMTEXT("[debugss] program type: %s"),
        tDebugSnapshot.strProgramType
      );
      DBSS_MSG(strString);
    }
  }

  if(!uRes) {
    OEMSNPRINTF(
      strString, MAX_PATH, OEMTEXT("[debugss] Debug data ver: %s"),
      tDebugSnapshot.strVersion
    );
    DBSS_MSG(strString);
    OEMSNPRINTF(
      strString, MAX_PATH, OEMTEXT("[debugss] Program ver: %s"),
        OEMTEXT(NP2VER_CORE) OEMTEXT(" ") OEMTEXT(NP2VER_GIT)
    );
    DBSS_MSG(strString);
    if(milstr_cmp(tDebugSnapshot.strVersion, OEMTEXT(NP2VER_CORE) OEMTEXT(" ") OEMTEXT(NP2VER_GIT)) != 0) {
      DBSS_MSG("[debugss] not match version.");
      uRes = 4;
    }
  }

  if(!uRes) {
    OEMSNPRINTF(
      strString, MAX_PATH, OEMTEXT("[debugss] FDD mount flag is %X."),
      tDebugSnapshot.uFDMount
    );
    DBSS_MSG(strString);
    for(iVal = 0; iVal < MAX_FDDFILE; iVal++) {
      if(tDebugSnapshot.uFDMount & (1 << iVal)) {
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("[debugss] FDD %d info."),
          iVal
        );
        DBSS_MSG(strString);
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("  Type: %d"),
          tDebugSnapshot.auFDType[iVal]
        );
        DBSS_MSG(strString);
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("  FilePath: %s"),
          tDebugSnapshot.astrFDImagePath[iVal]
        );
        DBSS_MSG(strString);
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("  ReadOnly: %d"),
          tDebugSnapshot.auFDRO[iVal]
        );
        DBSS_MSG(strString);
        OEMSNPRINTF(strString, MAX_PATH, OEMTEXT("%s"), OEMTEXT("  SHA-1: "));
        DBSS_MSG(strString);
        strString[0] = '\0';
        for(iVal2 = 0; iVal2 < 20; iVal2++) {
          OEMSNPRINTF(
            strFilePath, MAX_PATH, OEMTEXT("%02x"),
            tDebugSnapshot.au8FDHash[iVal][iVal2]
          );
          milstr_ncat(strString, strFilePath, MAX_PATH);
        }
        DBSS_MSG(strString);
      }
    }

    OEMSNPRINTF(
      strString, MAX_PATH, OEMTEXT("[debugss] HDCDD mount flag is %X."),
      tDebugSnapshot.uHDCDMount
    );
    DBSS_MSG(strString);
    for(iVal = 0; iVal < SASIHDD_MAX + SCSIHDD_MAX; iVal++) {
      if(tDebugSnapshot.uHDCDMount & (1 << iVal)) {
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("[debugss] HDCDD %d info."),
          iVal
        );
        DBSS_MSG(strString);
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("  Type: %d"),
          tDebugSnapshot.auHDCDType[iVal]
        );
        DBSS_MSG(strString);
        OEMSNPRINTF(
          strString, MAX_PATH, OEMTEXT("  FilePath: %s"),
          tDebugSnapshot.astrHDCDImagePath[iVal]
        );
        DBSS_MSG(strString);
        if(tDebugSnapshot.auHDCDType[iVal] == SXSIDEV_CDROM) {
          OEMSNPRINTF(strString, MAX_PATH, OEMTEXT("%s"), OEMTEXT("  SHA-1: "));
          DBSS_MSG(strString);
          strString[0] = '\0';
          for(iVal2 = 0; iVal2 < 20; iVal2++) {
            OEMSNPRINTF(
              strFilePath, MAX_PATH, OEMTEXT("%02x"),
              tDebugSnapshot.au8CDHash[iVal][iVal2]
            );
            milstr_ncat(strString, strFilePath, MAX_PATH);
          }
          DBSS_MSG(strString);
        }
      }
    }
  }

  if(!uRes) {
    memcpy(&np2cfg, &tDebugSnapshot.np2cfg, sizeof(NP2CFG));
    memcpy(&np2oscfg, &tDebugSnapshot.np2oscfg, sizeof(NP2OSCFG));
  }

  if(!uRes) {
    statsave_load(tDebugSnapshot.strStatePath);
  }

  return uRes;
}


