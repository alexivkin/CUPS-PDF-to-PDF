/* cups-pdf.h -- CUPS Backend Header File (version 3.0.1, 2017-02-24)
   16.05.2003, Volker C. Behr
   volker@cups-pdf.de
   http://www.cups-pdf.de


   This code may be freely distributed as long as this header 
   is preserved. Changes to the code should be clearly indicated.   

   This code is distributed under the GPL.
   (http://www.gnu.org/copyleft/gpl.html)

   For more detailed licensing information see cups-pdf.c in the 
   corresponding version number.			             */


/* User-customizable settings - if unsure leave the default values 
/  they are reasonable for most systems.			     */

/* location of the configuration file */
#define CP_CONFIG_PATH "/etc/cups"


/* --- DO NOT EDIT BELOW THIS LINE --- */

/* The following settings are for internal purposes only - all relevant 
/  options listed below can be set via cups-pdf.conf at runtime		*/

#define CPVERSION "v3.0.1"

#define CPERROR         1
#define CPSTATUS        2
#define CPDEBUG         4

#define BUFSIZE 4096
#define TBUFSIZE "4096"

typedef char cp_string[BUFSIZE];


#define SEC_CONF  1
#define SEC_PPD   2
#define SEC_LPOPT 4

/* order in the enum and the struct-array has to be identical! */

enum configOptions { AnonDirName, AnonUser, GhostScript, GSCall, Grp, GSTmp, Log, PDFVer, PostProcessing, Out, Spool, UserPrefix, RemovePrefix, OutExtension, Cut, Truncate, DirPrefix, Label, LogType, LowerCase, TitlePref, DecodeHexStrings, FixNewlines, AllowUnsafeOptions, AnonUMask, UserUMask, END_OF_OPTIONS };

struct {
  char *key_name;
  int security;
  union {
    cp_string sval;
    int ival;
    mode_t modval;
  } value;
} configData[] = {
  { "AnonDirName", SEC_CONF|SEC_PPD, { "/var/spool/cups-pdf/ANONYMOUS" } },
  { "AnonUser", SEC_CONF|SEC_PPD, { "nobody" } },
  { "GhostScript", SEC_CONF|SEC_PPD, { "/usr/bin/gs" } },
  { "GSCall", SEC_CONF|SEC_PPD, { "%s -q -dCompatibilityLevel=%s -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pdfwrite -sOutputFile=\"%s\" -dAutoRotatePages=/PageByPage -dAutoFilterColorImages=false -dColorImageFilter=/FlateEncode -dPDFSETTINGS=/prepress -c .setpdfwrite -f %s" } },
  { "Grp", SEC_CONF|SEC_PPD, { "lp" } },
  { "GSTmp", SEC_CONF|SEC_PPD, { "TMPDIR=/var/tmp" } },
  { "Log", SEC_CONF|SEC_PPD, { "/var/log/cups" } },
  { "PDFVer", SEC_CONF|SEC_PPD|SEC_LPOPT, { "1.4" } },
  { "PostProcessing", SEC_CONF|SEC_PPD|SEC_LPOPT, { "" } },
  { "Out", SEC_CONF|SEC_PPD, { "/var/spool/cups-pdf/${USER}" } },
  { "Spool", SEC_CONF|SEC_PPD, { "/var/spool/cups-pdf/SPOOL" } },
  { "UserPrefix", SEC_CONF|SEC_PPD, { "" } },
  { "RemovePrefix", SEC_CONF|SEC_PPD, { "" } },
  { "OutExtension", SEC_CONF|SEC_PPD|SEC_LPOPT, { "pdf" } },
  { "Cut", SEC_CONF|SEC_PPD|SEC_LPOPT, {{ 3 }} },
  { "Truncate", SEC_CONF|SEC_PPD|SEC_LPOPT, {{ 64 }} },
  { "DirPrefix", SEC_CONF|SEC_PPD, {{ 0 }} },
  { "Label", SEC_CONF|SEC_PPD|SEC_LPOPT, {{ 0 }} },
  { "LogType", SEC_CONF|SEC_PPD, {{ 3 }} },
  { "LowerCase", SEC_CONF|SEC_PPD, {{ 1 }} },
  { "TitlePref", SEC_CONF|SEC_PPD|SEC_LPOPT, {{ 0 }} },
  { "DecodeHexStrings", SEC_CONF|SEC_PPD, {{ 0 }} },
  { "FixNewlines", SEC_CONF|SEC_PPD, {{ 0 }} },
  { "AllowUnsafeOptions", SEC_CONF|SEC_PPD, {{ 0 }} },
  { "AnonUmask", SEC_CONF|SEC_PPD, {{ 0000 }} },
  { "UserUMask", SEC_CONF|SEC_PPD|SEC_LPOPT, {{ 0077 }} },
};

#define Conf_AnonDirName          configData[AnonDirName].value.sval
#define Conf_AnonUser             configData[AnonUser].value.sval
#define Conf_GhostScript          configData[GhostScript].value.sval
#define Conf_GSCall               configData[GSCall].value.sval
#define Conf_Grp                  configData[Grp].value.sval
#define Conf_GSTmp                configData[GSTmp].value.sval
#define Conf_Log                  configData[Log].value.sval
#define Conf_PDFVer               configData[PDFVer].value.sval
#define Conf_PostProcessing       configData[PostProcessing].value.sval
#define Conf_Out                  configData[Out].value.sval
#define Conf_Spool                configData[Spool].value.sval
#define Conf_UserPrefix           configData[UserPrefix].value.sval
#define Conf_RemovePrefix         configData[RemovePrefix].value.sval
#define Conf_OutExtension         configData[OutExtension].value.sval
#define Conf_Cut                  configData[Cut].value.ival
#define Conf_Truncate             configData[Truncate].value.ival
#define Conf_DirPrefix            configData[DirPrefix].value.ival
#define Conf_Label                configData[Label].value.ival
#define Conf_LogType              configData[LogType].value.ival
#define Conf_LowerCase            configData[LowerCase].value.ival
#define Conf_TitlePref            configData[TitlePref].value.ival
#define Conf_DecodeHexStrings     configData[DecodeHexStrings].value.ival
#define Conf_FixNewlines          configData[FixNewlines].value.ival
#define Conf_AllowUnsafeOptions   configData[AllowUnsafeOptions].value.ival
#define Conf_AnonUMask            configData[AnonUMask].value.modval
#define Conf_UserUMask            configData[UserUMask].value.modval
