#define IN_ADA_BODY
#include "dev_falc_spr.h"

typedef integer_8 dev_falc_spr__lw_falc_sec_spr_disable_exceptions_field;
enum {dev_falc_spr__clear=0, dev_falc_spr__set=1};
typedef struct _dev_falc_spr__lw_falc_sec_spr_register {
  unsigned_32 _pad1 : 19;
  unsigned_32 disable_exceptions : 1;
  unsigned_32 _pad2 : 12;
} GNAT_PACKED dev_falc_spr__lw_falc_sec_spr_register;
typedef integer_8 dev_falc_spr__lw_falc_csw_spr_enable_disable_fields;
enum {dev_falc_spr__disable=0, dev_falc_spr__enable=1};
typedef lw_types__Tlwu3B dev_falc_spr__Tlw_falc_csw_spr_clvl_fieldB;
typedef dev_falc_spr__Tlw_falc_csw_spr_clvl_fieldB dev_falc_spr__lw_falc_csw_spr_clvl_field;
typedef struct _dev_falc_spr__lw_falc_csw_spr_register {
  unsigned_32 _pad1 : 16;
  unsigned_32 f_ie0 : 1;
  unsigned_32 f_ie1 : 1;
  unsigned_32 f_ie2 : 1;
  unsigned_32 _pad2 : 5;
  unsigned_32 f_exception : 1;
  unsigned_32 _pad3 : 1;
  unsigned_32 f_clvl : 3;
  unsigned_32 _pad4 : 3;
} GNAT_PACKED dev_falc_spr__lw_falc_csw_spr_register;
typedef character dev_falc_spr__Tlw_falc_sec_spr_disable_exceptions_fieldSS[8];
const dev_falc_spr__Tlw_falc_sec_spr_disable_exceptions_fieldSS dev_falc_spr__lw_falc_sec_spr_disable_exceptions_fieldS
  = "CLEARSET";
typedef integer_8 dev_falc_spr__Tlw_falc_sec_spr_disable_exceptions_fieldNT[3];
const dev_falc_spr__Tlw_falc_sec_spr_disable_exceptions_fieldNT dev_falc_spr__lw_falc_sec_spr_disable_exceptions_fieldN
  = {1, 6, 9};
typedef character dev_falc_spr__Tlw_falc_csw_spr_enable_disable_fieldsSS[13];
const dev_falc_spr__Tlw_falc_csw_spr_enable_disable_fieldsSS dev_falc_spr__lw_falc_csw_spr_enable_disable_fieldsS =
  "DISABLEENABLE";
typedef integer_8 dev_falc_spr__Tlw_falc_csw_spr_enable_disable_fieldsNT[3];
const dev_falc_spr__Tlw_falc_csw_spr_enable_disable_fieldsNT dev_falc_spr__lw_falc_csw_spr_enable_disable_fieldsN = {
  1, 8, 14};
