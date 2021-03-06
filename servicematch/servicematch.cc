/***************************************************************************
 * servicematch.cc -- A relatively simple utility for determining whether  *
 * a given Nmap service fingerprint matches (or comes close to any of the  *
 * fingerprints in a collection such as the nmap-service-probes file that  *
 * ships with Nmap.                                                        *
 *                                                                         *
 ***********************IMPORTANT NMAP LICENSE TERMS************************
 *                                                                         *
 * The Nmap Security Scanner is (C) 1996-2009 Insecure.Com LLC. Nmap is    *
 * also a registered trademark of Insecure.Com LLC.  This program is free  *
 * software; you may redistribute and/or modify it under the terms of the  *
 * GNU General Public License as published by the Free Software            *
 * Foundation; Version 2 with the clarifications and exceptions described  *
 * below.  This guarantees your right to use, modify, and redistribute     *
 * this software under certain conditions.  If you wish to embed Nmap      *
 * technology into proprietary software, we sell alternative licenses      *
 * (contact sales@insecure.com).  Dozens of software vendors already       *
 * license Nmap technology such as host discovery, port scanning, OS       *
 * detection, and version detection.                                       *
 *                                                                         *
 * Note that the GPL places important restrictions on "derived works", yet *
 * it does not provide a detailed definition of that term.  To avoid       *
 * misunderstandings, we consider an application to constitute a           *
 * "derivative work" for the purpose of this license if it does any of the *
 * following:                                                              *
 * o Integrates source code from Nmap                                      *
 * o Reads or includes Nmap copyrighted data files, such as                *
 *   nmap-os-db or nmap-service-probes.                                    *
 * o Executes Nmap and parses the results (as opposed to typical shell or  *
 *   execution-menu apps, which simply display raw Nmap output and so are  *
 *   not derivative works.)                                                *
 * o Integrates/includes/aggregates Nmap into a proprietary executable     *
 *   installer, such as those produced by InstallShield.                   *
 * o Links to a library or executes a program that does any of the above   *
 *                                                                         *
 * The term "Nmap" should be taken to also include any portions or derived *
 * works of Nmap.  This list is not exclusive, but is meant to clarify our *
 * interpretation of derived works with some common examples.  Our         *
 * interpretation applies only to Nmap--we don't speak for other people's  *
 * GPL works.                                                              *
 *                                                                         *
 * If you have any questions about the GPL licensing restrictions on using *
 * Nmap in non-GPL works, we would be happy to help.  As mentioned above,  *
 * we also offer alternative license to integrate Nmap into proprietary    *
 * applications and appliances.  These contracts have been sold to dozens  *
 * of software vendors, and generally include a perpetual license as well  *
 * as providing for priority support and updates as well as helping to     *
 * fund the continued development of Nmap technology.  Please email        *
 * sales@insecure.com for further information.                             *
 *                                                                         *
 * As a special exception to the GPL terms, Insecure.Com LLC grants        *
 * permission to link the code of this program with any version of the     *
 * OpenSSL library which is distributed under a license identical to that  *
 * listed in the included COPYING.OpenSSL file, and distribute linked      *
 * combinations including the two. You must obey the GNU GPL in all        *
 * respects for all of the code used other than OpenSSL.  If you modify    *
 * this file, you may extend this exception to your version of the file,   *
 * but you are not obligated to do so.                                     *
 *                                                                         *
 * If you received these files with a written license agreement or         *
 * contract stating terms other than the terms above, then that            *
 * alternative license agreement takes precedence over these comments.     *
 *                                                                         *
 * Source is provided to this software because we believe users have a     *
 * right to know exactly what a program is going to do before they run it. *
 * This also allows you to audit the software for security holes (none     *
 * have been found so far).                                                *
 *                                                                         *
 * Source code also allows you to port Nmap to new platforms, fix bugs,    *
 * and add new features.  You are highly encouraged to send your changes   *
 * to nmap-dev@insecure.org for possible incorporation into the main       *
 * distribution.  By sending these changes to Fyodor or one of the         *
 * Insecure.Org development mailing lists, it is assumed that you are      *
 * offering the Nmap Project (Insecure.Com LLC) the unlimited,             *
 * non-exclusive right to reuse, modify, and relicense the code.  Nmap     *
 * will always be available Open Source, but this is important because the *
 * inability to relicense code has caused devastating problems for other   *
 * Free Software projects (such as KDE and NASM).  We also occasionally    *
 * relicense the code to third parties as discussed above.  If you wish to *
 * specify special license conditions of your contributions, just say so   *
 * when you send them.                                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * General Public License v2.0 for more details at                         *
 * http://www.gnu.org/licenses/gpl-2.0.html , or in the COPYING file       *
 * included with Nmap.                                                     *
 *                                                                         *
 ***************************************************************************/

/* $Id: servicematch.cc 4113 2006-11-01 23:31:18Z fyodor $ */


#include "nbase.h"
#include "nmap.h"
#include "nmap_error.h"
#include "service_scan.h"
#include "utils.h"

#include <ctype.h>

void usage() {
  printf("Usage: servicematch <fingerprintfilename>\n"
         "(You will be prompted for the fingerprint data)\n"
         "\n");
  exit(1);
}

/* Convert non-printable characters to replchar in the string. Assumes that we
   already allocated memory for the escaping characters. */
void replacenonprintableandescape(char *str, int strlength, char replchar, char escape_char) {
  int i;

  for (i = 0; i < strlength; i++) {
    if (!isprint((int)(unsigned char)str[i]))
      str[i] = replchar;
    if (str[i] == escape_char) {
      str[i] = '\\';
      i++;
      str[i] = escape_char;
    }
  }

  return;
}


char *cstringSanityCheck_escaped(const char* string, int len, char escape_char) {
  char *result;
  int slen, i;

  if(!string)
	  return NULL;

  slen = strlen(string);
  if (slen > len) slen = len;

  for (i = 0; i < slen; i++) {
    if (string[i] == escape_char) {
      slen++;
    }
  }

  result = (char *) safe_malloc(slen + 1);
  memcpy(result, string, slen);
  result[slen] = '\0';
  replacenonprintableandescape(result, slen, '.', '|');
  return result;
}


int print_sanitized(const char* format, const char* arg1, int len, char escape_char) {
    char* sanitized = cstringSanityCheck_escaped(arg1, len, escape_char);
    int ret = printf(format, sanitized);
    free(sanitized);
    return ret;
}

char *fallback_depth_translate(ServiceProbe *SP, int fallbackDepth) {
  char buf[1024];
  if (fallbackDepth == 0) return (char *)""; // Not a fallback
  snprintf(buf, sizeof(buf), "(FALLBACK: %s) ", SP->fallbacks[fallbackDepth]->getName());
  return strdup(buf); // leak but who cares?
}

// This function parses the read-in fprint, compares the responses to the
// given tests as if they had been read from a remote system, and prints out
// all matches, followed by the fingerprint in single-line format.
// The 'ipaddystr' is either a string of the form " on www.xx.y.zzz" containing the IP
// address of the target from which fprint was obtained, or it is empty (meaning we don't know).
int doMatch(AllProbes *AP, char *fprint, int fplen, char *ipaddystr) {
  int proto;
  char *p;
  char *currentprobe = NULL;
  char probename[128];
  char resptext[8192];
  char *endp = NULL;
  unsigned long fullrlen;
  bool trunc = false; // Was at least one response truncated due to length?
  bool found = false; // Was at least one match found?
  unsigned int resptextlen;
  char *dst;
  ServiceProbe *SP = NULL;
  const struct MatchDetails *MD;

  // First lets find the port number and protocol
  assert(fplen > 10);
  assert(strncmp(fprint, "SF-Port", 7) == 0);
  // Skip port number.
  p = strchr(fprint, ':');
  assert(p);
  p -= 3;
  if (strncmp(p, "TCP", 3) == 0)
    proto = IPPROTO_TCP;
  else proto = IPPROTO_UDP;

  currentprobe = strstr(p, "%r(");
  while(currentprobe) {
    // move to the probe name
    p = currentprobe + 3;
    dst = probename;
    while(*p && *p != ',') {
      assert((unsigned int) (dst - probename) < sizeof(probename) - 1);
      *dst++ = *p++;
    }
    *dst++ = '\0';

    // Grab
    assert(*p == ',');
    p++;
    assert(isxdigit(*p));
    fullrlen = strtoul(p, &endp, 16);
    p = endp;
    assert(*p == ',');
    p++;
    assert(*p == '"');
    p++;

    dst = resptext;
    while(*p && *p != '"') {
      if (*p == '\\') {
        // if it's an escape, grab this and the next one
      assert((unsigned int) (dst - resptext) < sizeof(resptext) - 1);
      *dst++ = *p++;
    }
      assert((unsigned int) (dst - resptext) < sizeof(resptext) - 1);
      *dst++ = *p++;
    }
    *dst++ = '\0';

    // Now we unescape the response into plain binary
    cstring_unescape(resptext, &resptextlen);

    if (resptextlen < fullrlen)
      trunc = true;
    else if (resptextlen > fullrlen)
      fatal("Error: decoded response length 0x%X exceeds recorded response length 0x%lX.", resptextlen, fullrlen);

    // Finally we try to match this with the appropriate probe from the
    // nmap-service-probes file.
    SP = AP->getProbeByName(probename, proto);

    if (!SP) {
      error("WARNING: Unable to find probe named %s in given probe file.", probename);
    } else {
      int fallbackDepth, n;
      bool hard = false; // Has at least one hard match been seen?
      for (fallbackDepth = 0; SP->fallbacks[fallbackDepth] != NULL; fallbackDepth++) {
        for (n = 0; (MD = SP->fallbacks[fallbackDepth]->testMatch((const u8 *)resptext, resptextlen, n)) != NULL; n++) {
          if (!MD->isSoft && MD->serviceName != NULL) {
            // YEAH!  Found a hard match!
            hard = true;
            found = true;
            if (MD->product || MD->version || MD->info || MD->hostname || MD->ostype || MD->devicetype) {
              printf("MATCHED %s:%d %ssvc %s", probename, MD->lineno, fallback_depth_translate(SP, fallbackDepth), MD->serviceName);

              if (MD->product) print_sanitized(" p|%s|", MD->product, 80, '|');
              if (MD->version) print_sanitized(" v|%s|", MD->version, 80, '|');
              if (MD->info) print_sanitized(" i|%s|", MD->info, 256, '|');
              if (MD->hostname) print_sanitized(" h|%s|", MD->hostname, 80, '|');
              if (MD->ostype) print_sanitized(" o|%s|", MD->ostype, 32, '|');
              if (MD->devicetype) print_sanitized(" d|%s|", MD->devicetype, 32, '|');

              if (MD->cpe_a) print_sanitized(" %s", MD->cpe_a, 80, '|');
              if (MD->cpe_h) print_sanitized(" %s", MD->cpe_h, 80, '|');
              if (MD->cpe_o) print_sanitized(" %s", MD->cpe_o, 80, '|');

              printf(" %s\n", ipaddystr);
            } else
              printf("MATCHED %s:%d %ssvc %s (NO VERSION)%s\n", probename, MD->lineno, fallback_depth_translate(SP, fallbackDepth), MD->serviceName, ipaddystr);
          }
        }
        if (!hard) {
          for (n = 0; (MD = SP->fallbacks[fallbackDepth]->testMatch((const u8 *)resptext, resptextlen, n)) != NULL; n++) {
            if (MD->serviceName != NULL) {
              found = true;
              printf("SOFT MATCH %s:%d svc %s (SOFT MATCH)%s\n", probename, MD->lineno, MD->serviceName, ipaddystr);
            }
          }
        }
      }
    }
    // Lets find the next probe, if any
    currentprobe = strstr(p, "%r(");
  }

  if (trunc) printf("WARNING:  At least one probe response was truncated (%lu vs %u)\n", fullrlen, resptextlen);
  if (!found) printf("FAILED to match%s\n", ipaddystr);
  printf("DONE\n");

  return found ? 0 : 1;
}

int cleanfp(char *fprint, int *fplen) {
  char *src = fprint, *dst = fprint;

  while(*src) {
    if (strncmp(src, "\\x20", 4) == 0) {
      *dst++ = ' ';
      src += 4;
      /* } else if (*src == '\\' && (*(src+1) == '"' || *(src+1) == '\\')) {
      *dst++ = *++src;
      src++; */ // We shouldn't do this yet
    } else if (src != dst) {
      *dst++ = *src++;
    } else { dst++; src++; }
  }
  *dst++ = '\0';
  *fplen = dst - fprint - 1;
  return 0;
}


int main(int argc, char *argv[]) {
  AllProbes *AP = new AllProbes();
  char *probefile = NULL;
  int fplen = 0; // Amount of chars in the current fprint
  char line[51200];
  char fprint[sizeof(line)*32];
  unsigned int linelen;
  char *dst = NULL;
  int lineno = 0;
  char *p, *q;
  bool isInFP = false; // whether we are currently reading in a fingerprint
  struct in_addr ip;
  char lastipbuf[64];

  if (argc != 2)
    usage();

  lastipbuf[0] = '\0';

  /* First we read in the fingerprint file provided on the command line */
  probefile = argv[1];
  parse_nmap_service_probe_file(AP, probefile);

  /* Now we read in the user-provided service fingerprint(s) */

  printf("Enter the service fingerprint(s) you would like to match.  Will read until EOF.  Other Nmap output text (besides fingerprints) is OK too and will be ignored\n");

  while(fgets(line, sizeof(line), stdin)) {
    lineno++;
    linelen = strlen(line);
    p = line;
    while(*p && isspace(*p)) p++;
    if (isInFP) {
      if (strncmp(p, "SF:", 3) == 0) {
        p += 3;
        assert(sizeof(fprint) > fplen + linelen + 1);
        dst = fprint + fplen;
        while(*p != '\r' && *p != '\n' && *p != ' ')
          *dst++ = *p++;
        fplen = dst - fprint;
        *dst++ = '\0';
      } else {
        fatal("Fingerprint incomplete ending on line #%d", lineno);
      }
    }

    if (strncmp(p, "SF-Port", 7) == 0) {
      if (isInFP)
        fatal("New service fingerprint started before the previous one was complete -- line %d", lineno);
      assert(sizeof(fprint) > linelen + 1);
      dst = fprint;
      while(*p != '\r' && *p != '\n' && *p != ' ')
        *dst++ = *p++;
      fplen = dst - fprint;
      *dst++ = '\0';
      isInFP = true;
    } else if (strncmp(p, "Interesting port", 16) == 0) {
      q = line + linelen - 1;
      while(*q && (*q == ')' || *q == ':' || *q == '\n'|| *q == '.' || isdigit((int) (unsigned char) *q))) {
        if (*q == ')' || *q == ':' || *q == '\n') *q = '\0';
        q--;
      }
      q++;
      assert(isdigit((int)(unsigned char) *q));
      if (inet_aton(q, &ip) != 0) {
        snprintf(lastipbuf, sizeof(lastipbuf), " on %s", inet_ntoa(ip));
      }
    }

    // Now we test if the fingerprint is complete
    if (isInFP && fplen > 5 && strncmp(fprint + fplen - 3, "\");", 3) == 0) {
      // Yeah!  We have read in the whole fingerprint
      isInFP = false;
      // Cleans the fingerprint up a little, such as replacing \x20 with space and unescaping characters like \\ and \"
      cleanfp(fprint, &fplen);
      doMatch(AP, fprint, fplen, lastipbuf);
    }
  }

  return 0;
}
