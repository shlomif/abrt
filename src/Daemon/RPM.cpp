/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "abrtlib.h"
#include "RPM.h"
#include <rpm/header.h>
#include <rpm/rpmts.h>
#include "comm_layer_inner.h"

CRPM::CRPM()
{
    int status = rpmReadConfigFiles((const char*)NULL, (const char*)NULL);
    if (status != 0)
        error_msg("error reading rc files");
}

CRPM::~CRPM()
{
    rpmFreeRpmrc();
}

void CRPM::LoadOpenGPGPublicKey(const char* pFileName)
{
    const uint8_t* pkt = NULL;
    size_t pklen;
    if (pgpReadPkts(pFileName, &pkt, &pklen) != PGPARMOR_PUBKEY)
    {
        free((void*)pkt);
        error_msg("Can't load public GPG key %s", pFileName);
        return;
    }

    uint8_t keyID[8];
    if (pgpPubkeyFingerprint(pkt, pklen, keyID) == 0)
    {
        char* fedoraFingerprint = pgpHexStr(keyID, sizeof(keyID));
        if (fedoraFingerprint != NULL)
            m_setFingerprints.insert(fedoraFingerprint);
    }
    free((void*)pkt);
}

bool CRPM::CheckFingerprint(const char* pPackage)
{
    bool ret = false;
    char *pgpsig = NULL;
    const char *errmsg = NULL;

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage, 0);
    Header header = rpmdbNextIterator(iter);

    if (!header)
        goto error;

    pgpsig = headerSprintf(header, "%{SIGGPG:pgpsig}", rpmTagTable, rpmHeaderFormats, &errmsg);
    if (!pgpsig && errmsg)
    {
        VERB1 log("cannot get siggpg:pgpsig. reason: %s", errmsg);
        goto error;
    }

    {
        char *pgpsig_tmp = strstr(pgpsig, " Key ID ");
        if (pgpsig_tmp)
        {
            pgpsig_tmp += sizeof(" Key ID ") - 1;
            if (m_setFingerprints.find(pgpsig_tmp) != m_setFingerprints.end())
                ret = true;
        }
    }

error:
    free(pgpsig);
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}

/*
  Checking the MD5 sum requires to run prelink to "un-prelink" the
  binaries - this is considered potential security risk so we don't
  use it, until we find some non-intrusive way
*/

/*
 * Not woking, need to be rewriten
 *
bool CheckHash(const char* pPackage, const char* pPath)
{
    bool ret = true;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pPackage, 0);
    Header header = rpmdbNextIterator(iter);
    if (header == NULL)
        goto error;

    rpmfi fi = rpmfiNew(ts, header, RPMTAG_BASENAMES, RPMFI_NOHEADER);
    pgpHashAlgo hashAlgo;
    std::string headerHash;
    char computedHash[1024] = "";

    while (rpmfiNext(fi) != -1)
    {
        if (strcmp(pPath, rpmfiFN(fi)) == 0)
        {
            headerHash = rpmfiFDigestHex(fi, &hashAlgo);
            rpmDoDigest(hashAlgo, pPath, 1, (unsigned char*) computedHash, NULL);
            ret = (headerHash != "" && headerHash == computedHash);
            break;
        }
    }
    rpmfiFree(fi);
error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}
*/

char* rpm_get_description(const char* pkg)
{
    char *dsc = NULL;
    const char *errmsg = NULL;
    rpmts ts = rpmtsCreate();

    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pkg, 0);
    Header header = rpmdbNextIterator(iter);
    if (!header)
	goto error;

    dsc = headerSprintf(header, "%{SUMMARY}\n\n%{DESCRIPTION}", rpmTagTable, rpmHeaderFormats, &errmsg);
    if (!dsc && errmsg)
        error_msg("cannot get summary and description. reason: %s", errmsg);

error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return dsc;
}

char* rpm_get_component(const char* filename)
{
    char *ret = NULL;
    char *srpm = NULL;
    const char *errmsg = NULL;

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, filename, 0);
    Header header = rpmdbNextIterator(iter);
    if (!header)
        goto error;

    srpm = headerSprintf(header, "%{SOURCERPM}", rpmTagTable, rpmHeaderFormats, &errmsg);
    if (!srpm && errmsg)
    {
        error_msg("cannot get srpm. reason: %s", errmsg);
        goto error;
    }

    ret = get_package_name_from_NVR_or_NULL(srpm);
    free(srpm);

error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return ret;
}


// caller is responsible to free returned value
char* rpm_get_package_nvr(const char* filename)
{
    char* nvr = NULL;
    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_BASENAMES, filename, 0);
    Header header = rpmdbNextIterator(iter);

    const char *np, *vp, *rp;
    if (!header)
        goto error;

    // returns alway 0
    headerNVR(header, &np, &vp, &rp);
    nvr = xasprintf("%s-%s-%s", np, vp, rp);

error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);
    return nvr;
}
