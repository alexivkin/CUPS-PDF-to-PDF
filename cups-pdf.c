/* cups-pdf.c -- CUPS Backend (version 3.0.1, 2017-02-24)
   08.02.2003, Volker C. Behr
   volker@cups-pdf.de
   http://www.cups-pdf.de

   This code may be freely distributed as long as this header
   is preserved.

   This code is distributed under the GPL.
   (http://www.gnu.org/copyleft/gpl.html)

   ---------------------------------------------------------------------------

   Copyright (C) 2003-2017  Volker C. Behr

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   ---------------------------------------------------------------------------

   If you want to redistribute modified sources/binaries this header
   has to be preserved and all modifications should be clearly
   indicated.
   In case you want to include this code into your own programs
   I would appreciate your feedback via email.


   HISTORY: see ChangeLog in the parent directory of the source archive
*/

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stddef.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/backend.h>

#include "cups-pdf.h"


static FILE *logfp=NULL;
int input_is_pdf=0;


static void log_event(short type, const char *message, ...) {
  time_t secs;
  int error=errno;
  char ctype[8], *timestring;
  cp_string logbuffer;
  va_list ap;

  if ((logfp != NULL) && (type & Conf_LogType)) {
    (void) time(&secs);
    timestring=ctime(&secs);
    timestring[strlen(timestring)-1]='\0';

    if (type == CPERROR)
      snprintf(ctype, 8, "ERROR");
    else if (type == CPSTATUS)
      snprintf(ctype, 8, "STATUS");
    else
      snprintf(ctype, 8, "DEBUG");

    va_start(ap, message);
    vsnprintf(logbuffer, BUFSIZE, message, ap);
    va_end(ap);

    fprintf(logfp,"%s  [%s] %s\n", timestring, ctype, logbuffer);
    if ((Conf_LogType & CPDEBUG) && (type == CPERROR) && error)
      fprintf(logfp,"%s  [DEBUG] ERRNO: %d (%s)\n", timestring, error, strerror(error));

    (void) fflush(logfp);
  }

  return;
}

static int create_dir(char *dirname, int nolog) {
  struct stat fstatus;
  char buffer[BUFSIZE],*delim;
  int i;

  while ((i=strlen(dirname))>1 && dirname[i-1]=='/')
    dirname[i-1]='\0';
  if (stat(dirname, &fstatus) || !S_ISDIR(fstatus.st_mode)) {
    strncpy(buffer,dirname,BUFSIZE);
    delim=strrchr(buffer,'/');
    if (delim!=buffer)
      delim[0]='\0';
    else
      delim[1]='\0';
    if (create_dir(buffer,nolog)!=0)
      return 1;
    (void) stat(buffer, &fstatus);
    if (mkdir(dirname,fstatus.st_mode)!=0) {
      if (!nolog)
        log_event(CPERROR, "failed to create directory: %s", dirname);
      return 1;
    }
    else
      if (!nolog)
        log_event(CPSTATUS, "directory created: %s", dirname);
    if (chown(dirname,fstatus.st_uid,fstatus.st_gid)!=0)
      if (!nolog)
        log_event(CPDEBUG, "failed to set owner on directory: %s (non fatal)", dirname);
  }
  return 0;
}

static int _assign_value(int security, char *key, char *value) {
  int tmp;
  int option;

  for (option=0; option<END_OF_OPTIONS; option++) {
    if (!strcasecmp(key, configData[option].key_name))
      break;
  }

  if (option == END_OF_OPTIONS) {
    return 0;
  }

  if (!(security & configData[option].security) && !(Conf_AllowUnsafeOptions)) {
    log_event(CPERROR, "Unsafe option not allowed: %s", key);
    return 0;
  }

  switch(option) {
    case AnonDirName:
           strncpy(Conf_AnonDirName, value, BUFSIZE);
           break;
    case AnonUser:
           strncpy(Conf_AnonUser, value, BUFSIZE);
           break;
    case GhostScript:
           strncpy(Conf_GhostScript, value, BUFSIZE);
           break;
    case GSCall:
           strncpy(Conf_GSCall, value, BUFSIZE);
           break;
    case Grp:
           strncpy(Conf_Grp, value, BUFSIZE);
           break;
    case GSTmp:
           snprintf(Conf_GSTmp, BUFSIZE, "%s%s", "TMPDIR=", value);
           break;
    case Log:
           strncpy(Conf_Log, value, BUFSIZE);
           break;
    case PDFVer:
           strncpy(Conf_PDFVer, value, BUFSIZE);
           break;
    case PostProcessing:
           strncpy(Conf_PostProcessing, value, BUFSIZE);
           break;
    case Out:
           strncpy(Conf_Out, value, BUFSIZE);
           break;
    case Spool:
           strncpy(Conf_Spool, value, BUFSIZE);
           break;
    case UserPrefix:
           strncpy(Conf_UserPrefix, value, BUFSIZE);
           break;
    case RemovePrefix:
           strncpy(Conf_RemovePrefix, value, BUFSIZE);
           break;
    case Cut:
          tmp=atoi(value);
          Conf_Cut=(tmp>=-1)?tmp:-1;
          break;
    case Truncate:
          tmp=atoi(value);
          Conf_Truncate=(tmp>=8)?tmp:8;
          break;
    case DirPrefix:
          tmp=atoi(value);
          Conf_DirPrefix=(tmp)?1:0;
          break;
    case Label:
          tmp=atoi(value);
          Conf_Label=(tmp>2)?2:((tmp<0)?0:tmp);
          break;
    case LogType:
          tmp=atoi(value);
          Conf_LogType=(tmp>7)?7:((tmp<0)?0:tmp);
          break;
    case LowerCase:
          tmp=atoi(value);
          Conf_LowerCase=(tmp)?1:0;
          break;
    case TitlePref:
          tmp=atoi(value);
          Conf_TitlePref=(tmp)?1:0;
          break;
    case DecodeHexStrings:
          tmp=atoi(value);
          Conf_DecodeHexStrings=(tmp)?1:0;
          break;
    case FixNewlines:
          tmp=atoi(value);
          Conf_FixNewlines=(tmp)?1:0;
          break;
    case AnonUMask:
          tmp=(int)strtol(value,NULL,8);
          Conf_AnonUMask=(mode_t)tmp;
          break;
    case UserUMask:
          tmp=(int)strtol(value,NULL,8);
          Conf_UserUMask=(mode_t)tmp;
          break;
    default:
          log_event(CPERROR, "Program error: option not treated: %s = %s\n", key, value);
          return 0;
  }
  return 1;
}

static void read_config_file(char *filename) {
  FILE *fp=NULL;
  struct stat fstatus;
  cp_string buffer, key, value;

  if ((strlen(filename) > 1) && (!stat(filename, &fstatus)) &&
      (S_ISREG(fstatus.st_mode) || S_ISLNK(fstatus.st_mode))) {
    fp=fopen(filename,"r");
  }
  if (fp==NULL) {
    log_event(CPERROR, "Cannot open config: %s", filename);
    return;
  }

  while (fgets(buffer, BUFSIZE, fp) != NULL) {
    key[0]='\0';
    value[0]='\0';
    if (sscanf(buffer,"%s %[^\n]",key,value)) {
      if (!strlen(key) || !strncmp(key,"#",1))
        continue;
      _assign_value(SEC_CONF, key, value);
    }
  }

  (void) fclose(fp);
  return;
}

static void read_config_ppd() {
  ppd_option_t *option;
  ppd_file_t *ppd_file;
  char * ppd_name;

  ppd_name = getenv("PPD");
  if (ppd_name == NULL) {
    log_event(CPERROR, "Could not retrieve PPD name");
    return;
  }
  ppd_file = ppdOpenFile(ppd_name);
  if (ppd_file == NULL) {
    log_event(CPERROR, "Could not open PPD file: %s", ppd_name);
    return;
  }
  ppdMarkDefaults(ppd_file);

  option = ppdFirstOption(ppd_file);
  while (option != NULL) {
    _assign_value(SEC_PPD, option->keyword, option->defchoice);
    option = ppdNextOption(ppd_file);
  }
  ppdClose(ppd_file);

  return;
}

static void read_config_options(const char *lpoptions) {
  int i;
  int num_options;
  cups_option_t *options;
  cups_option_t *option;

  num_options = cupsParseOptions(lpoptions, 0, &options);

  for (i = 0, option = options; i < num_options; i ++, option ++) {

    /* replace all _ by " " in value */
    int j;
    for (j=0; option->value[j]!= '\0'; j++) {
      if (option->value[j] == '_') {
        option->value[j] = ' ';
      }
    }
    _assign_value(SEC_LPOPT, option->name, option->value);
  }
  return;
}

static void dump_configuration() {
  if (Conf_LogType & CPDEBUG) {
    log_event(CPDEBUG, "*** Final Configuration ***");
    log_event(CPDEBUG, "AnonDirName        = \"%s\"", Conf_AnonDirName);
    log_event(CPDEBUG, "AnonUser           = \"%s\"", Conf_AnonUser);
    log_event(CPDEBUG, "GhostScript        = \"%s\"", Conf_GhostScript);
    log_event(CPDEBUG, "GSCall             = \"%s\"", Conf_GSCall);
    log_event(CPDEBUG, "Grp                = \"%s\"", Conf_Grp);
    log_event(CPDEBUG, "GSTmp              = \"%s\"", Conf_GSTmp);
    log_event(CPDEBUG, "Log                = \"%s\"", Conf_Log);
    log_event(CPDEBUG, "PDFVer             = \"%s\"", Conf_PDFVer);
    log_event(CPDEBUG, "PostProcessing     = \"%s\"", Conf_PostProcessing);
    log_event(CPDEBUG, "Out                = \"%s\"", Conf_Out);
    log_event(CPDEBUG, "Spool              = \"%s\"", Conf_Spool);
    log_event(CPDEBUG, "UserPrefix         = \"%s\"", Conf_UserPrefix);
    log_event(CPDEBUG, "RemovePrefix       = \"%s\"", Conf_RemovePrefix);
    log_event(CPDEBUG, "OutExtension       = \"%s\"", Conf_OutExtension);
    log_event(CPDEBUG, "Cut                = %d", Conf_Cut);
    log_event(CPDEBUG, "Truncate           = %d", Conf_Truncate);
    log_event(CPDEBUG, "DirPrefix          = %d", Conf_DirPrefix);
    log_event(CPDEBUG, "Label              = %d", Conf_Label);
    log_event(CPDEBUG, "LogType            = %d", Conf_LogType);
    log_event(CPDEBUG, "LowerCase          = %d", Conf_LowerCase);
    log_event(CPDEBUG, "TitlePref          = %d", Conf_TitlePref);
    log_event(CPDEBUG, "DecodeHexStrings   = %d", Conf_DecodeHexStrings);
    log_event(CPDEBUG, "FixNewlines        = %d", Conf_FixNewlines);
    log_event(CPDEBUG, "AllowUnsafeOptions = %d", Conf_AllowUnsafeOptions);
    log_event(CPDEBUG, "AnonUMask          = %04o", Conf_AnonUMask);
    log_event(CPDEBUG, "UserUMask          = %04o", Conf_UserUMask);
    log_event(CPDEBUG, "*** End of Configuration ***");
  }
  return;
}

static int init(char *argv[]) {
  struct stat fstatus;
  struct group *group;
  cp_string filename;
  int grpstat;
  const char *uri=cupsBackendDeviceURI(argv);

  if ((uri != NULL) && (strncmp(uri, "cups-pdf:/", 10) == 0) && strlen(uri) > 10) {
    uri = uri + 10;
    sprintf(filename, "%s/cups-pdf-%s.conf", CP_CONFIG_PATH, uri);
  }
  else {
    sprintf(filename, "%s/cups-pdf.conf", CP_CONFIG_PATH);
  }
  read_config_file(filename);

  read_config_ppd();

  read_config_options(argv[5]);

  (void) umask(0077);

  group=getgrnam(Conf_Grp);
  grpstat=setgid(group->gr_gid);

  if (strlen(Conf_Log)) {
    if (stat(Conf_Log, &fstatus) || !S_ISDIR(fstatus.st_mode)) {
      if (create_dir(Conf_Log, 1))
        return 1;
      if (chmod(Conf_Log, 0700))
        return 1;
    }
    snprintf(filename, BUFSIZE, "%s/%s%s%s", Conf_Log, "cups-pdf-", getenv("PRINTER"), "_log");
    logfp=fopen(filename, "a");
  }

  dump_configuration();

  if (!group) {
    log_event(CPERROR, "Grp not found: %s", Conf_Grp);
    return 1;
  }
  else if (grpstat) {
    log_event(CPERROR, "failed to set new gid: %s", Conf_Grp);
    return 1;
  }
  else
    log_event(CPDEBUG, "set new gid: %s", Conf_Grp);

  (void) umask(0022);

  if (stat(Conf_Spool, &fstatus) || !S_ISDIR(fstatus.st_mode)) {
    if (create_dir(Conf_Spool, 0)) {
      log_event(CPERROR, "failed to create spool directory: %s", Conf_Spool);
      return 1;
    }
    if (chmod(Conf_Spool, 0751)) {
      log_event(CPERROR, "failed to set mode on spool directory: %s", Conf_Spool);
      return 1;
    }
    if (chown(Conf_Spool, -1, group->gr_gid))
      log_event(CPERROR, "failed to set group id %s on spool directory: %s (non fatal)", Conf_Grp, Conf_Spool);
    log_event(CPSTATUS, "spool directory created: %s", Conf_Spool);
  }

  (void) umask(0077);
  return 0;
}

static void announce_printers() {
  DIR *dir;
  struct dirent *config_ent;
  int len;
  cp_string setup;

  printf("file cups-pdf:/ \"Virtual PDF Printer\" \"CUPS-PDF\" \"MFG:Generic;MDL:CUPS-PDF Printer;DES:Generic CUPS-PDF Printer;CLS:PRINTER;CMD:PDF,POSTSCRIPT;\"\n");

  if ((dir = opendir(CP_CONFIG_PATH)) != NULL) {
    while ((config_ent = readdir(dir)) != NULL) {
      len = strlen(config_ent->d_name);
      if ((strncmp(config_ent->d_name, "cups-pdf-", 9) == 0) &&
          (len > 14 && strcmp(config_ent->d_name + len - 5, ".conf") == 0)) {
        strncpy(setup, config_ent->d_name + 9, BUFSIZE>len-14 ? len-14 : BUFSIZE);
        setup[BUFSIZE>len-14 ? len-14 : BUFSIZE - 1] = '\0';
        printf("file cups-pdf:/%s \"Virtual %s Printer\" \"CUPS-PDF\" \"MFG:Generic;MDL:CUPS-PDF Printer;DES:Generic CUPS-PDF Printer;CLS:PRINTER;CMD:PDF,POSTSCRIPT;\"\n", setup, setup);
      }
    }
    closedir(dir);
  }
  return;
}

static char *preparedirname(struct passwd *passwd, char *uname) {
  int size;
  char bufin[BUFSIZE], bufout[BUFSIZE], *needle, *cptr;

  needle=strstr(uname, Conf_RemovePrefix);
  if ((int)strlen(uname)>(size=strlen(Conf_RemovePrefix)))
    uname=uname+size;

  strncpy(bufin, Conf_Out, BUFSIZE);
  do {
    needle=strstr(bufin, "${HOME}");
    if (needle == NULL)
      break;
    needle[0]='\0';
    cptr=needle+7;
    snprintf(bufout, BUFSIZE, "%s%s%s", bufin, passwd->pw_dir, cptr);
    strncpy(bufin, bufout, BUFSIZE);
  } while (needle != NULL);
  do {
    needle=strstr(bufin, "${USER}");
    if (needle == NULL)
      break;
    needle[0]='\0';
    cptr=needle+7;
    if (!Conf_DirPrefix)
      snprintf(bufout, BUFSIZE, "%s%s%s", bufin, uname, cptr);
    else
      snprintf(bufout, BUFSIZE, "%s%s%s", bufin, passwd->pw_name, cptr);
    strncpy(bufin, bufout, BUFSIZE);
  } while (needle != NULL);
  size=strlen(bufin)+1;
  cptr=calloc(size, sizeof(char));
  if (cptr == NULL)
    return NULL;
  snprintf(cptr,size,"%s",bufin);
  return cptr;
}

static int prepareuser(struct passwd *passwd, char *dirname) {
  struct stat fstatus;

  (void) umask(0000);
  if (stat(dirname, &fstatus) || !S_ISDIR(fstatus.st_mode)) {
    if (!strcmp(passwd->pw_name, Conf_AnonUser)) {
      if (create_dir(dirname, 0)) {
        log_event(CPERROR, "failed to create anonymous output directory: %s", dirname);
        return 1;
      }
      if (chmod(dirname, (mode_t)(0777&~Conf_AnonUMask))) {
        log_event(CPERROR, "failed to set mode on anonymous output directory: %s", dirname);
        return 1;
      }
      log_event(CPDEBUG, "anonymous output directory created: %s", dirname);
    }
    else {
      if (create_dir(dirname, 0)) {
        log_event(CPERROR, "failed to create user output directory: %s", dirname);
        return 1;
      }
      if (chmod(dirname, (mode_t)(0777&~Conf_UserUMask))) {
        log_event(CPERROR, "failed to set mode on user output directory: %s", dirname);
        return 1;
      }
      log_event(CPDEBUG, "user output directory created: %s", dirname);
    }
    if (chown(dirname, passwd->pw_uid, passwd->pw_gid)) {
      log_event(CPERROR, "failed to set owner for output directory: %s", passwd->pw_name);
      return 1;
    }
    log_event(CPDEBUG, "owner set for output directory: %s", passwd->pw_name);
  }
  (void) umask(0077);
  return 0;
}

/* no validation is done here, please use is_ps_hex_string for that */
static void decode_ps_hex_string(char *string) {
  char *src_ptr, *dst_ptr;
  int is_lower_digit;                   /* 0 - higher digit, 1 - lower digit */
  char number, digit;

  dst_ptr=string;                   /* we should always be behind src_ptr,
                                       so it's safe to write over original string */
  number=(char)0;
  is_lower_digit=0;
  for (src_ptr=string+1;*src_ptr != '>';src_ptr++) {    /* begin after start marker */
    if (*src_ptr == ' ' || *src_ptr == '\t' ) {     /* skip whitespace */
      continue;
    }
    if (*src_ptr >= 'a') {              /* assuming 0 < A < a */
      digit=*src_ptr-'a'+(char)10;
    }
    else if (*src_ptr >= 'A') {
      digit=*src_ptr-'A'+(char)10;
    }
    else {
      digit=*src_ptr-'0';
    }
    if (is_lower_digit) {
      number|=digit;
      *dst_ptr=number;                  /* write character */
      dst_ptr++;
      is_lower_digit=0;
    }
    else {                      /* higher digit */
      number=digit<<4;
      is_lower_digit=1;
    }
  }
  if (is_lower_digit) {                 /* write character with lower digit = 0,
                                   as per PostScript Language Reference */
    *dst_ptr=number;
    dst_ptr++;
    /* is_lower_digit=0; */
  }
  *dst_ptr=0;                       /* finish him! */
  return;
}

static int is_ps_hex_string(char *string) {
  int got_end_marker=0;
  char *ptr;

  if (string[0] != '<') {   /* if has no start marker */
    log_event(CPDEBUG, "not a hex string, has no start marker: %s", string);
    return 0;                   /* not hex string, obviously */
  }
  for (ptr=string+1;*ptr;ptr++) {   /* begin after start marker */
    if (got_end_marker) {       /* got end marker and still something left */
      log_event(CPDEBUG, "not a hex string, trailing characters after end marker: %s", ptr);
      return 0;                 /* that's bad! */
    }
    else if (*ptr == '>') {     /* here it is! */
      got_end_marker=1;
      log_event(CPDEBUG, "got an end marker in the hex string, expecting 0-termination: %s", ptr);
    }
    else if ( !(
      isxdigit(*ptr) ||
      *ptr == ' ' ||
      *ptr == '\t'
    ) ) {
      log_event(CPDEBUG, "not a hex string, invalid character: %s", ptr);
      return 0;             /* that's bad, too */
    }
  }
  return got_end_marker;
}

static void alternate_replace_string(char *string) {
  unsigned int i;

  log_event(CPDEBUG, "removing alternate special characters from title: %s", string);
  for (i=0;i<(unsigned int)strlen(string);i++)
   if ( isascii(string[i]) &&       /* leaving non-ascii characters intact */
        (!isalnum(string[i])) &&
        string[i] != '-' && string[i] != '+' && string[i] != '.')
    string[i]='_';
  return;
}

static void replace_string(char *string) {
  unsigned int i;

  log_event(CPDEBUG, "removing special characters from title: %s", string);
  for (i=0;i<(unsigned int)strlen(string);i++)
    if ( ( string[i] < '0' || string[i] > '9' ) &&
         ( string[i] < 'A' || string[i] > 'Z' ) &&
         ( string[i] < 'a' || string[i] > 'z' ) &&
         string[i] != '-' && string[i] != '+' && string[i] != '.')
      string[i]='_';
  return;
}

static int preparetitle(char *title) {
  char *cut;
  int i;

  if (title != NULL) {
    if (Conf_DecodeHexStrings) {
      log_event(CPSTATUS, "***Experimental Option: DecodeHexStrings");
      log_event(CPDEBUG, "checking for hex strings: %s", title);
      if (is_ps_hex_string(title))
        decode_ps_hex_string(title);
      log_event(CPDEBUG, "calling alternate_replace_string");
      alternate_replace_string(title);
    }
    else {
      replace_string(title);
    }
    i=strlen(title);
    if (i>1) {
      while (title[--i]=='_');
      if (i<strlen(title)-1) {
        log_event(CPDEBUG, "removing trailing _ from title: %s", title);
        title[i+1]='\0';
      }
      i=0;
      while (title[i++]=='_');
      if (i>1) {
        log_event(CPDEBUG, "removing leading _ from title: %s", title);
        memmove(title, title+i-1, strlen(title)-i+2);
      }
    }
    while (strlen(title)>2 && title[0]=='(' && title[strlen(title)-1]==')') {
      log_event(CPDEBUG, "removing enclosing parentheses () from full title: %s", title);
      title[strlen(title)-1]='\0';
      memmove(title, title+1, strlen(title));
    }
  }
  cut=strrchr(title, '/');
  if (cut != NULL) {
    log_event(CPDEBUG, "removing slashes from full title: %s", title);
    memmove(title, cut+1, strlen(cut+1)+1);
  }
  cut=strrchr(title, '\\');
  if (cut != NULL) {
    log_event(CPDEBUG, "removing backslashes from full title: %s", title);
    memmove(title, cut+1, strlen(cut+1)+1);
  }
  cut=strrchr(title, '.');
  if ((cut != NULL) && ((int)strlen(cut) <= Conf_Cut+1) && (cut != title)) {
    log_event(CPDEBUG, "removing file name extension: %s", cut);
    cut[0]='\0';
  }
  if (strlen(title)>Conf_Truncate) {
    title[Conf_Truncate]='\0';
    log_event(CPDEBUG, "truncating title: %s", title);
  }
  return strcmp(title, "");
}

static char *fgets2(char *fbuffer, int fbufsize, FILE *ffpsrc) {
  /* like fgets() but linedelimiters are 0x0A, 0x0C, 0x0D (LF, FF, CR). */
  int c, pos;
  char *result;

  if (!Conf_FixNewlines)
    return fgets(fbuffer, fbufsize, ffpsrc);

  result=NULL;
  pos=0;

  while (pos < fbufsize) {      /* pos in [0..fbufsize-1] */
    c=fgetc(ffpsrc);          /* converts CR/LF to LF in some OSses */
    if (c == EOF)           /* EOF _or_ error */
      break;
    fbuffer[pos++]=c;
    if (c == 0x0A || c == 0x0C || c == 0x0D) /* line is at an end */
      break;
  }

  if (pos > 0 && !ferror(ffpsrc)) { /* at least one char read and no error */
    fbuffer[pos]='\0';
    result=fbuffer;
  }

  return result;
}

static int preparespoolfile(FILE *fpsrc, char *spoolfile, char *title, char *cmdtitle,
                     int job, struct passwd *passwd) {
  cp_string buffer;
  int rec_depth,is_title=0;
  FILE *fpdest;
  size_t bytes = 0;

  if (fpsrc == NULL) {
    log_event(CPERROR, "failed to open source stream");
    return 1;
  }
  log_event(CPDEBUG, "source stream ready");
  fpdest=fopen(spoolfile, "w");
  if (fpdest == NULL) {
    log_event(CPERROR, "failed to open spoolfile: %s", spoolfile);
    (void) fclose(fpsrc);
    return 1;
  }
  log_event(CPDEBUG, "destination stream ready: %s", spoolfile);
  if (chown(spoolfile, passwd->pw_uid, -1)) {
    log_event(CPERROR, "failed to set owner for spoolfile: %s", spoolfile);
    return 1;
  }
  log_event(CPDEBUG, "owner set for spoolfile: %s", spoolfile);
  rec_depth=0;
  if (Conf_FixNewlines)
    log_event(CPSTATUS, "***Experimental Option: FixNewlines");
  else
    log_event(CPDEBUG, "using traditional fgets");

  while (fgets2(buffer, BUFSIZE, fpsrc) != NULL) {
    if (!strncmp(buffer, "%PDF", 4)) {
      log_event(CPDEBUG, "found beginning of PDF code", buffer);
      input_is_pdf=1;
      break;
    }
    if (!strncmp(buffer, "%!", 2) && strncmp(buffer, "%!PS-AdobeFont", 14)) {
      log_event(CPDEBUG, "found beginning of postscript code: %s", buffer);
      break;
    }
  }

  (void) fputs(buffer, fpdest);

  if (input_is_pdf) {
    while((bytes = fread(buffer, sizeof(char), BUFSIZE, fpsrc)) > 0)
      fwrite(buffer, sizeof(char), bytes, fpdest);
  } else {
    log_event(CPDEBUG, "now extracting postscript code");
    while (fgets2(buffer, BUFSIZE, fpsrc) != NULL) {
      (void) fputs(buffer, fpdest);
      if (!is_title && !rec_depth)
        if (sscanf(buffer, "%%%%Title: %"TBUFSIZE"c", title)==1) {
          log_event(CPDEBUG, "found title in ps code: %s", title);
          is_title=1;
        }
      if (!strncmp(buffer, "%!", 2)) {
        log_event(CPDEBUG, "found embedded (e)ps code: %s", buffer);
        rec_depth++;
      }
      else if (!strncmp(buffer, "%%EOF", 5)) {
        if (!rec_depth) {
          log_event(CPDEBUG, "found end of postscript code: %s", buffer);
          break;
        }
        else {
          log_event(CPDEBUG, "found end of embedded (e)ps code: %s", buffer);
          rec_depth--;
        }
      }
    }
  }

  (void) fclose(fpdest);
  (void) fclose(fpsrc);
  log_event(CPDEBUG, "all data written to spoolfile: %s", spoolfile);

  if (cmdtitle == NULL || !strcmp(cmdtitle, "(stdin)"))
    buffer[0]='\0';
  else
    strncpy(buffer, cmdtitle, BUFSIZE);
  if (title == NULL || !strcmp(title, "((stdin))"))
    title[0]='\0';

  if (Conf_TitlePref) {
    log_event(CPDEBUG, "trying to use commandline title: %s", buffer);
    if (!preparetitle(buffer)) {
      log_event(CPDEBUG, "empty commandline title, using PS title: %s", title);
      if (!preparetitle(title))
        log_event(CPDEBUG, "empty PS title");
    }
    else
      snprintf(title, BUFSIZE, "%s", buffer);
  }
  else {
    log_event(CPDEBUG, "trying to use PS title: %s", title);
    if (!preparetitle(title)) {
      log_event(CPDEBUG, "empty PS title, using commandline title: %s", buffer);
      if (!preparetitle(buffer))
        log_event(CPDEBUG, "empty commandline title");
      else
        snprintf(title, BUFSIZE, "%s", buffer);
    }
  }

  if (!strcmp(title, "")) {
    if (Conf_Label == 2)
      snprintf(title, BUFSIZE, "untitled_document-job_%i", job);
    else
      snprintf(title, BUFSIZE, "job_%i-untitled_document", job);
    log_event(CPDEBUG, "no title found - using default value: %s", title);
  }
  else {
    if (Conf_Label) {
      strcpy(buffer, title);
      if (Conf_Label == 2)
        snprintf(title, BUFSIZE, "%s-job_%i", buffer, job);
      else
        snprintf(title, BUFSIZE, "job_%i-%s", job, buffer);
    }
    log_event(CPDEBUG, "title successfully retrieved: %s", title);
  }
  return 0;
}

int main(int argc, char *argv[]) {
  char *user, *dirname, *spoolfile, *outfile, *gscall, *ppcall;
  cp_string title;
  int size;
  mode_t mode;
  struct passwd *passwd;
  gid_t *groups;
  int ngroups;
  pid_t pid;

  if (setuid(0)) {
    (void) fputs("CUPS-PDF cannot be called without root privileges!\n", stderr);
    return 0;
  }

  if (argc==1) {
    announce_printers();
    return 0;
  }
  if (argc<6 || argc>7) {
    (void) fputs("Usage: cups-pdf job-id user title copies options [file]\n", stderr);
    return 0;
  }

  if (init(argv))
    return 5;
  log_event(CPDEBUG, "initialization finished: %s", CPVERSION);

  size=strlen(Conf_UserPrefix)+strlen(argv[2])+1;
  user=calloc(size, sizeof(char));
  if (user == NULL) {
    (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
    return 5;
  }
  snprintf(user, size, "%s%s", Conf_UserPrefix, argv[2]);
  passwd=getpwnam(user);
  if (passwd == NULL && Conf_LowerCase) {
    log_event(CPDEBUG, "unknown user: %s", user);
    for (size=0;size<(int) strlen(argv[2]);size++)
      argv[2][size]=tolower(argv[2][size]);
    log_event(CPDEBUG, "trying lower case user name: %s", argv[2]);
    size=strlen(Conf_UserPrefix)+strlen(argv[2])+1;
    snprintf(user, size, "%s%s", Conf_UserPrefix, argv[2]);
    passwd=getpwnam(user);
  }
  if (passwd == NULL) {
    if (strlen(Conf_AnonUser)) {
      passwd=getpwnam(Conf_AnonUser);
      if (passwd == NULL) {
        log_event(CPERROR, "username for anonymous access unknown: %s", Conf_AnonUser);
        free(user);
        if (logfp!=NULL)
          (void) fclose(logfp);
        return 5;
      }
      log_event(CPDEBUG, "unknown user: %s", user);
      size=strlen(Conf_AnonDirName)+4;
      dirname=calloc(size, sizeof(char));
      if (dirname == NULL) {
        (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
        free(user);
        if (logfp!=NULL)
          (void) fclose(logfp);
        return 5;
      }
      snprintf(dirname, size, "%s", Conf_AnonDirName);
      while (strlen(dirname) && ((dirname[strlen(dirname)-1] == '\n') ||
             (dirname[strlen(dirname)-1] == '\r')))
        dirname[strlen(dirname)-1]='\0';
      log_event(CPDEBUG, "output directory name generated: %s", dirname);
    }
    else {
      log_event(CPSTATUS, "anonymous access denied: %s", user);
      free(user);
      if (logfp!=NULL)
        (void) fclose(logfp);
      return 0;
    }
    mode=(mode_t)(0666&~Conf_AnonUMask);
  }
  else {
    log_event(CPDEBUG, "user identified: %s", passwd->pw_name);
    if ((dirname=preparedirname(passwd, argv[2])) == NULL) {
      (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
      free(user);
      if (logfp!=NULL)
        (void) fclose(logfp);
      return 5;
    }
    while (strlen(dirname) && ((dirname[strlen(dirname)-1] == '\n') ||
           (dirname[strlen(dirname)-1] == '\r')))
      dirname[strlen(dirname)-1]='\0';
    log_event(CPDEBUG, "output directory name generated: %s", dirname);
    mode=(mode_t)(0666&~Conf_UserUMask);
  }
  ngroups=32;
  groups=calloc(ngroups, sizeof(gid_t));
  if (groups == NULL) {
    (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
    free(user);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  size=getgrouplist(user, passwd->pw_gid, groups, &ngroups);
  if (size == -1) {
    free(groups);
    groups=calloc(ngroups, sizeof(gid_t));
    size=getgrouplist(user, passwd->pw_gid, groups, &ngroups);
  }
  if (size < 0) {
    log_event(CPERROR, "getgrouplist failed");
    free(user);
    free(groups);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  free(user);
  if (prepareuser(passwd, dirname)) {
    free(groups);
    free(dirname);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  log_event(CPDEBUG, "user information prepared");

  size=strlen(Conf_Spool)+22;
  spoolfile=calloc(size, sizeof(char));
  if (spoolfile == NULL) {
    (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
    free(groups);
    free(dirname);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  snprintf(spoolfile, size, "%s/cups2pdf-%i", Conf_Spool, (int) getpid());
  log_event(CPDEBUG, "spoolfile name created: %s", spoolfile);

  if (argc == 6) {
    if (preparespoolfile(stdin, spoolfile, title, argv[3], atoi(argv[1]), passwd)) {
      free(groups);
      free(dirname);
      free(spoolfile);
      if (logfp!=NULL)
        (void) fclose(logfp);
      return 5;
    }
    log_event(CPDEBUG, "input data read from stdin");
  }
  else {
    if (preparespoolfile(fopen(argv[6], "r"), spoolfile, title, argv[3], atoi(argv[1]), passwd)) {
      free(groups);
      free(dirname);
      free(spoolfile);
      if (logfp!=NULL)
        (void) fclose(logfp);
      return 5;
    }
    log_event(CPDEBUG, "input data read from file: %s", argv[6]);
  }

  size=strlen(dirname)+strlen(title)+strlen(Conf_OutExtension)+3;
  outfile=calloc(size, sizeof(char));
  if (outfile == NULL) {
    (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
    if (unlink(spoolfile))
      log_event(CPERROR, "failed to unlink spoolfile during clean-up: %s", spoolfile);
    free(groups);
    free(dirname);
    free(spoolfile);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  if (strlen(Conf_OutExtension))
    snprintf(outfile, size, "%s/%s.%s", dirname, title, Conf_OutExtension);
  else
    snprintf(outfile, size, "%s/%s", dirname, title);
  log_event(CPDEBUG, "output filename created: %s", outfile);

  size=strlen(Conf_GSCall)+strlen(Conf_GhostScript)+strlen(Conf_PDFVer)+strlen(outfile)+strlen(spoolfile)+6;
  gscall=calloc(size, sizeof(char));
  if (gscall == NULL) {
    (void) fputs("CUPS-PDF: failed to allocate memory\n", stderr);
    if (unlink(spoolfile))
      log_event(CPERROR, "failed to unlink spoolfile during clean-up: %s", spoolfile);
    free(groups);
    free(dirname);
    free(spoolfile);
    free(outfile);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  if (input_is_pdf) {
    snprintf(gscall, size, "cp %s %s", spoolfile, outfile);
  } else {
    snprintf(gscall, size, Conf_GSCall, Conf_GhostScript, Conf_PDFVer, outfile, spoolfile);
  }

  log_event(CPDEBUG, "ghostscript commandline built: %s", gscall);

  (void) unlink(outfile);
  log_event(CPDEBUG, "output file unlinked: %s", outfile);

  if (putenv(Conf_GSTmp)) {
    log_event(CPERROR, "insufficient space in environment to set TMPDIR: %s", Conf_GSTmp);
    if (unlink(spoolfile))
      log_event(CPERROR, "failed to unlink spoolfile during clean-up: %s", spoolfile);
    free(groups);
    free(dirname);
    free(spoolfile);
    free(outfile);
    free(gscall);
    if (logfp!=NULL)
      (void) fclose(logfp);
    return 5;
  }
  log_event(CPDEBUG, "TMPDIR set for GhostScript: %s", getenv("TMPDIR"));

  pid=fork();

  if (!pid) {
    log_event(CPDEBUG, "entering child process");

    if (setgid(passwd->pw_gid))
      log_event(CPERROR, "failed to set GID for current user");
    else
      log_event(CPDEBUG, "GID set for current user");
    if (setgroups(ngroups, groups))
      log_event(CPERROR, "failed to set supplementary groups for current user");
    else
      log_event(CPDEBUG, "supplementary groups set for current user");
    if (setuid(passwd->pw_uid))
      log_event(CPERROR, "failed to set UID for current user: %s", passwd->pw_name);
    else
      log_event(CPDEBUG, "UID set for current user: %s", passwd->pw_name);

    (void) umask(0077);
    size=system(gscall);
    log_event(CPDEBUG, "ghostscript has finished: %d", size);
    if (chmod(outfile, mode))
      log_event(CPERROR, "failed to set file mode for PDF file: %s (non fatal)", outfile);
    else
      log_event(CPDEBUG, "file mode set for user output: %s", outfile);

    if (strlen(Conf_PostProcessing)) {
      size=strlen(Conf_PostProcessing)+strlen(outfile)+strlen(passwd->pw_name)+strlen(argv[2])+4;
      ppcall=calloc(size, sizeof(char));
      if (ppcall == NULL)
        log_event(CPERROR, "failed to allocate memory for postprocessing (non fatal)");
      else {
        snprintf(ppcall, size, "%s %s %s %s", Conf_PostProcessing, outfile, passwd->pw_name, argv[2]);
        log_event(CPDEBUG, "postprocessing commandline built: %s", ppcall);
        size=system(ppcall);
        snprintf(title,BUFSIZE,"%d",size);
        log_event(CPDEBUG, "postprocessing has finished: %s", title);
        free(ppcall);
      }
    }
    else
     log_event(CPDEBUG, "no postprocessing");

    return 0;
  }
  log_event(CPDEBUG, "waiting for child to exit");
  (void) waitpid(pid,NULL,0);

  if (unlink(spoolfile))
    log_event(CPERROR, "failed to unlink spoolfile: %s (non fatal)", spoolfile);
  else
    log_event(CPDEBUG, "spoolfile unlinked: %s", spoolfile);

  free(groups);
  free(dirname);
  free(spoolfile);
  free(outfile);
  free(gscall);

  log_event(CPDEBUG, "all memory has been freed");

  log_event(CPSTATUS, "PDF creation successfully finished for %s", passwd->pw_name);

  if (logfp!=NULL)
    (void) fclose(logfp);
  return 0;
}
