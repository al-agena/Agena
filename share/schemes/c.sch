object TPersHolder
  P.Name = 'C'
  P.SyntaxBlocks = <
    item
      Name = 'Default'
      ID = 0
      CaseSensitive = True
      UseMetaSymbol = True
      UseMetaToWrapLines = True
      MetaSymbol = '\'
      FIText = 0
      FIIntNum = 1
      FIFloatNum = 2
      FIHexNum = 3
      FIDirective = 4
      FISymbol = 11
      UseSymbols = True
      UseLineDirectives = True
      UseComments = True
      UseSingleLineComments = True
      UseMultiLineComments = True
      UseStrings = True
      UseSingleLineStrings = True
      UseNumbers = True
      UsePrefixedNumbers = True
      UseSuffixedNumbers = True
      UseMultipleNumSuffixes = True
      UsePrefixedSuffixedNumbers = True
      UsePSNumComposition = True
      UseKeywords = True
      BlockDelimiters = <>
      LineDirectivePrefix = '#'
      SingleLineCommentDelimiters = <
        item
          FontID = 5
          LeftDelimiter = '//'
        end>
      MultiLineCommentDelimiters = <
        item
          FontID = 6
          LeftDelimiter = '/*'
          RightDelimiter = '*/'
        end>
      SingleLineStringDelimiters = <
        item
          FontID = 7
          LeftDelimiter = #39
          RightDelimiter = #39
        end
        item
          FontID = 7
          LeftDelimiter = '"'
          RightDelimiter = '"'
        end
        item
          FontID = 9
          LeftDelimiter = 'L"'
          RightDelimiter = '"'
        end>
      NumPrefixes = <
        item
          LeftDelimiter = '0x'
        end
        item
          LeftDelimiter = '0X'
        end>
      NumSuffixes = <
        item
          LeftDelimiter = 'l'
        end
        item
          LeftDelimiter = 'L'
        end
        item
          LeftDelimiter = 'u'
        end
        item
          LeftDelimiter = 'U'
        end>
      NumPrefixesSuffixes = <>
      KeywordSets = <
        item
          FontID = 8
          Name = 'logical'
          Keywords =
            '&&,||,!,!=,NULL,==,~=,<=,>='
        end
        item
          FontID = 10
          Name = 'reserved'
          Keywords =
            'abs,accept,acos,acosh,asinh,atan,atan2,atanh,auto,bind,break,calloc,case,cacos,cacosh,casin,casinh,catan,catanh,cadd,cabs,cbrt,' +
            'ccos,ccosh,cdecl,cexp,cimag,clearerr,clog,close,closedir,cmul,connect,continue,copysign,cpow,creal,csin,csinh,csqrt,csub,ctan,ctanh,' +
            'default,do,else,enum,exit,exp,extern,fabs,fabsl,fclose,feof,fflush,fileno,floor,fma,fmal,fmod,fopen,for,fortran,fprintf,' +
            'fpclassify,fputs,fread,free,frexp,fscanf,fseek,fstat,ftell,ftello,ftello64,ftruncate,fwrite,getc,getch,getcwd,goto,huge,hypot,' +
            'if,ilogb,inline,interrupt,isalpha,isalnum,iscntrl,isdigit,isinf,islower,isnan,isspace,ispunct,isupper,isunordered,isxdigit,kbhit,' +
            'ldexp,listen,localeconv,log,log1p,log2,log10,lseek,lseek64,malloc,memcmp,memcpy,memset,modf,near,open,opendir,pascal,pclose,popen,pow,' +
            'printf,read,readdir,realloc,realpath,register,return,round,setvbuf,signbit,sin,sinh,sizeof,socket,sprintf,sqrt,sqrtl,stat,strcat,strchr,' +
            'strcoll,strcpy,strcspn,strdup,strerror,strftime,strlen,strncat,strncmp,strncpy,strpbrk,strrchr,strspn,strstr,strtok,strtoul,strtoull,' +
            'switch,sysctlbyname,tan,tanh,time,tolower,toupper,uchar,ungetc,unlikely,unmount,unmount2,volatile,while,write'
        end
        item
          FontID = 12
          Name = 'OS2reserved'
          Keywords =
            'DosBeep,DosClose,DosDevIOCtl,DosGetDateTime,DosQueryCurrentDisk,DosQueryPathInfo,DosQuerySysInfo,DosOpen,' +
            'DosSetDateTime,DosSetPathInfo,DosShutdown'
        end
        item
          FontID = 12
          Name = 'Windowsreserved'
          Keywords =
            'CreateLink'
        end
        item
          FontID = 12
          Name = 'datatypes'
          Keywords =
            'bool,char,complex,const,div_t,double,false,float,int,int16_t,int32_t,int64_t,int8_t,lconv,long,near,off_t,off64_t,' +
            'pascal,ptrdiff_t,register,short,size_t,ssize_t,static,struct,typedef,time_t,true,uint16_t,uint32_t,uint64_t,uint8_t,' +
            'ULONG,union,unsigned,va_end,va_list,va_start,void'
        end
        item
          FontID = 12
          Name = 'datatypesWindows'
          Keywords =
            'BOOL,CPINFOEX,DWORD,HANDLE,HRESULT,LPCSTR,LPSTR,PREVENT_MEDIA_REMOVAL,UINT,ULONG,TCHAR,WCHAR'
        end
        item
          FontID = 12
          Name = 'datatypesOS2'
          Keywords =
            'APIRET,HFILE,PCSZ,UCHAR,ULONG'
        end
        item
          FontID = 13
          Name = 'luadatatypes'
          Keywords =
            'agn_Complex,BinOpr,BlockCnt,CallInfo,CClosure,Closure,Complex,ConsControl,DumpState,expdesc,FuncState,GCheader,GCObject,' +
            'global_State,L_Umaxalign,l_uacNumber,LClosure,LexState,LHS_assign,LNode,LocVar,LTKey,lu_byte,LUA_API,lua_CFunction,lua_Debug,' +
            'lua_Hook,lua_Integer,lua_Number,lua_State,lua_Unsigned,LUA_UNSIGNED,lua_Writer,LUALIB_API,MatchState,Node,Pair,Param,Proto,' +
            'Reg,SemInfo,Seq,StkId,Table,TKey,TString,TValue,Udata,UltraSet,UnOpr,UpVal,Value'
        end
        item
          FontID = 9
          Name = 'LuaFunctions'
          Keywords =
            'adjust_assign,adjustlocalvars,agenaV_addup,agenaV_addup_regsumup_div,agenaV_addup_seqsumup_div,agenaV_addup_sumup_div,' +
            'agenaV_approxcomppairs,agenaV_approxcompregsonebyone,agenaV_approxcompseqsonebyone,agenaV_approxcompusets,' +
            'agenaV_bottomindex,agenaV_comppairs,agenaV_compregs,agenaV_compregsonebyone,agenaV_compseqs,agenaV_compseqsonebyone,' +
            'agenaV_comptablesonebyone,agenaV_compusets,agenaV_copy,agenaV_count,agenaV_delete,agenaV_deletefrom,' +
            'agenaV_empty,agenaV_filled,agenaV_in,agenaV_insert,agenaV_instr,agenaV_intentries,agenaV_join,agenaV_mulup,' +
            'agenaV_nops,agenaV_numqmdev,agenaV_numsetops,agenaV_numunion,agenaV_qsumup_div,agenaV_regisofnumerictype,' +
            'agenaV_regqsumup_div,agenaV_regsumup,agenaV_replace,agenaV_seqisofnumerictype,agenaV_seqqsumup_div,' +
            'agenaV_seqsumup,agenaV_setmetatable,agenaV_setops,agenaV_setops_settodict,agenaV_setstorage,agenaV_setstore,' +
            'agenaV_subset,agenaV_sumup,agenaV_tentries,agenaV_times,agenaV_tintindices,agenaV_tisofnumerictype,' +
            'agenaV_toset,agenaV_tparts,agenaV_union,agenaV_unique,agenaV_usisofnumerictype,agenaV_values,agnG_indexerror,' +
            'agnH_aborders,agnH_asize,agnH_borders,agnH_clear,agnH_delete,luaH_findindex,luaH_free,luaH_get,luaH_gethashnum,luaH_getn,' +
            'luaH_getnum,agnH_hasarraypart,agnH_hashashpart,luaH_hashnum,agnH_hasholes,luaH_hashpointer,agnH_hsize,luaH_isdummy,' +
            'luaH_mainposition,luaH_next,agnH_nops,agnH_purge,agnH_readonly,agnH_reorder,agnH_rotatebottom,agnH_rotatetop,' +
            'luaH_set,luaH_setnum,luaH_setstr,agnH_subs,agnL_checkboolean,agnL_checkint,' +
            'agnL_checkinteger,agnL_checknumber,agnL_checkoption,agnL_createpairofnumbers,agnL_datetosecs,agnL_debuginfo,' +
            'agnL_debuginfo2,agnL_fillarray,agnL_fncall,agnL_fncallx,agnL_fneps,agnL_fnunicall,agnL_geti,agnL_getmetafield,' +
            'agnL_getsetting,agnL_gettablefield,agnL_gettop,agnL_initialise,agnL_iscallable,agnL_isdlong,agnL_islinalgvector,' +
            'agnL_isvalidpattern,agnL_onexit,agnL_optboolean,agnL_optint32_t,agnL_optinteger,agnL_optnonnegative,' +
            'agnL_optnonnegint,agnL_optnumber,agnL_optoff64_t,agnL_optposint,agnL_optpositive,agnL_optstring,agnL_optuint32_t,' +
            'agnL_paircheckbooloption,agnL_pairgeticomplex,agnL_pairgetiintegers,agnL_pairgetilongnumbers,agnL_pairgetinonnegint,' +
            'agnL_pairgetinonnegints,agnL_pairgetinumber,agnL_pairgetinumbers,agnL_pairgetiposints,agnL_pexecute,' +
            'agnL_printnonstruct,agnL_pushcoeffs,agnL_pushhex,agnL_pushvstring,agnL_readlines,agnL_setLibname,' +
            'agnL_strmatch,agnL_strtonumber,agnL_structisnumber,agnL_structisnumeric,agnL_structisstring,agnL_strunwrap,' +
            'agnL_tonumarray,agnL_tonumberx,agnL_tostringx,agnO_aligntowordboundary,agnO_log2,agnO_newsize,agnPair_create,' +
            'agnPair_free,agnPair_new,agnPair_rawgeti,agnPair_readonly,agnPair_seti,agnReg_delete,agnReg_duplicate,' +
            'agnReg_exchange,agnReg_free,agnReg_get,agnReg_geti,agnReg_new,agnReg_next,agnReg_purge,agnReg_rawgeti,agnReg_readonly,agnReg_reorder,' +
            'agnReg_resize,agnReg_rotatebottom,agnReg_rotatetop,agnReg_set,agnReg_seti,agnReg_settop,agnReg_subs,agnSeq_addi,agnSeq_bestsize,' +
            'agnSeq_delete,agnSeq_duplicate,agnSeq_exchange,agnSeq_free,agnSeq_get,agnSeq_geti,agnSeq_new,agnSeq_next,' +
            'agnSeq_rawgeti,agnSeq_rawseti,agnSeq_readonly,agnSeq_reorder,agnSeq_resize,agnSeq_rotatebottom,agnSeq_rotatetop,' +
            'agnSeq_seti,agnSeq_subs,agnUS_delete,agnUS_delstr,agnUS_free,agnUS_get,agnUS_getcomplex,agnUS_getnum,' +
            'agnUS_getstr,agnUS_new,agnUS_next,agnUS_nops,agnUS_readonly,agnUS_resize,agnUS_set,agnUS_setnum,agnUS_setstr,' +
            'agnUS_setstr2set,agn_arrayborders,agn_arrayorhashgeti,agn_arraypart,agn_arraytoseq,agn_asize,agn_auxcdate,' +
            'agn_borders,agn_call,agn_ccall,agn_char,agn_checkboolean,agn_checkcomplex,agn_checkinteger,agn_checklstring,' +
            'agn_checknonnegative,agn_checknonnegint,agn_checknonzeroint,agn_checknumber,agn_checkposint,agn_checkpositive,agn_checkstring,' +
            'agn_checkuint16_t,agn_checkuint32_t,agn_cleanse,agn_cleanseset,agn_clear,agn_complexgetimag,agn_complexgetreal,' +
            'agn_compleximag,agn_complexreal,agn_copy,agn_countitems,agn_createarray,agn_createcomplex,agn_createpair,' +
            'agn_createpairnumbers,agn_createpairstringnumber,agn_createpairstrings,agn_createreg,agn_creatertable,' +
            'agn_createseq,agn_createset,agn_deletefield,agn_deletefrom,agn_deletertable,agn_entries,agn_equalref,' +
            'agn_fb2int,agn_fnext,agn_freeze,agn_getbitwise,agn_getbuffersize,agn_getclosetozero,agn_getcmplxparts,' +
            'agn_getconstants,agn_getconstanttoobig,agn_getdblepsilon,agn_getdebug,agn_getdigits,agn_getduplicates,' +
            'agn_getemptyline,agn_getenclose,agn_getepsilon,agn_geterrmlinebreak,agn_getfeatures,agn_getfunctiontype,' +
            'agn_getgui,agn_gethepsilon,agn_gethsize,agn_getiinteger,agn_getilstring,agn_getinlinecache,agn_getinumber,' +
            'agn_getiso8601,agn_getkahanbabuska,agn_getkahanozawa,agn_getlibnamereset,agn_getlongtable,agn_getnoini,' +
            'agn_getnomainlib,agn_getpromptnewline,agn_getregsize,agn_getround,agn_getrtablewritemode,agn_getseqautoshrink,' +
            'agn_getseqlstring,agn_getskipagenapath,agn_getstarttime,agn_getstorage,agn_getstructuresize,agn_getthread,' +
            'agn_gettoken,agn_getutype,agn_getzeroedcomplex,agn_hashpart,agn_hsize,agn_in,agn_initmethodcall,agn_instr,' +
            'agn_int2fb,agn_intentries,agn_intindices,agn_isboolean,agn_isfail,agn_isfalse,agn_isfloat,agn_isinegint,' +
            'agn_isinteger,agn_isnegint,agn_isnonnegint,agn_isnumber,agn_isposint,agn_isstring,agn_istableutype,' +
            'agn_istrue,agn_isutype,agn_isutypeset,agn_join,agn_log2,agn_lower,agn_malloc,agn_mulup,agn_ncall,' +
            'agn_newsize,agn_nops,agn_numdiffset,agn_numintersect,agn_numminus,agn_numunion,agn_optcomplex,agn_pairgeti,' +
            'agn_pairrawget,agn_pairrawset,agn_pairset,agn_pairseti,agn_pairstate,agn_parts,agn_poptop,agn_poptoptwo,' +
            'agn_pushboolean,agn_pushcomplex,agn_qmdev,agn_qsumupdiv,agn_rawgetfield,agn_rawgeticomplex,agn_rawgetiinteger,agn_rawgetilstring,' +
            'agn_rawgetinumber,agn_rawinsert,agn_rawinsertfrom,agn_rawsetfield,agn_regdelete,agn_reggeti,agn_reggetinoerr,' +
            'agn_reggetinoerrrange,agn_reggetinumber,agn_reggettop,agn_regisall,agn_regisboolean,agn_regiscomplex,' +
            'agn_regisintegral,agn_regisnil,agn_regisnonnegative,agn_regisnonnegint,agn_regisnumber,agn_regisnumeric,' +
            'agn_regisposint,agn_regispositive,agn_regisstring,agn_regpurge,agn_regrawget,agn_regrawgeticomplex,' +
            'agn_regrawgetinumber,agn_regrawseti,agn_regresize,agn_regset,agn_regsettop,agn_regsize,agn_regstate,' +
            'agn_regsubs,agn_reorder,agn_replace,agn_resize,agn_sallocated,agn_seqgetinumber,agn_seqisall,agn_seqisboolean,' +
            'agn_seqiscomplex,agn_seqisintegral,agn_seqisnonnegative,agn_seqisnonnegint,agn_seqisnumber,agn_seqisnumeric,' +
            'agn_seqisposint,agn_seqispositive,agn_seqisstring,agn_seqrawgeticomplex,agn_seqrawgetiinteger,agn_seqrawgetilstring,' +
            'agn_seqrawgetinoerr,agn_seqrawgetinumber,agn_seqrawsetilstring,agn_seqresize,agn_seqsize,agn_seqstate,' +
            'agn_seqsubs,agn_setbitwise,agn_setbuffersize,agn_setclosetozero,agn_setconstants,agn_setconstanttoobig,' +
            'agn_setdblepsilon,agn_setdebug,agn_setdigits,agn_setduplicates,agn_setemptyline,agn_setenclose,agn_setepsilon,' +
            'agn_seterrmlinebreak,agn_setforadjust,agn_setgeqautoshrink,agn_setgui,agn_sethepsilon,agn_setinlinecache,' +
            'agn_setinumber,agn_setiso8601,agn_setkahanbabuska,agn_setkahanozawa,agn_setlibnamereset,agn_setlongtable,' +
            'agn_setnoini,agn_setnomainlib,agn_setpromptnewline,agn_setreadlibbed,agn_setregsize,agn_setresize,' +
            'agn_setround,agn_setrtable,agn_setseqautoshrink,agn_setskipagenapath,agn_setstorage,agn_setudmetatable,' +
            'agn_setutype,agn_setutypestring,agn_setzeroedcomplex,agn_size,agn_ssize,agn_sstate,agn_stralloc,agn_strmatch,' +
            'agn_structinsert,agn_sumup,agn_sumupdiv,agn_tablesize,agn_tablestate,agn_tabpurge,agn_tabresize,agn_tabsubs,' +
            'agn_tblisall,agn_times,agn_tocomplex,agn_tocomplexx,agn_tofileno,agn_tointeger,agn_tonumber,agn_tonumberx,' +
            'agn_toseq,agn_toset,agn_tostring,agn_trim,agn_udfreeze,agn_unique,agn_upper,agn_usedbytes,agn_usisall,' +
            'agn_usisboolean,agn_usiscomplex,agn_usisintegral,agn_usisnonnegative,agn_usisnonnegint,agn_usisnumber,' +
            'agn_usisnumeric,agn_usisposint,agn_usispositive,agn_usisstring,agn_values,allocparamspace,anchor_token,' +
            'api_check,api_checknelems,api_checkvalidindex,api_incr_top,api_rawgetiposrelat,api_rawgetirange,api_seqrawgeti,' +
            'api_seqrawgetinoerr,api_seqrawgetinoerrrange,api_seqrawgetinumber,assignment,block,block_follow,body,' +
            'break_or_continue,bvalue,call_binTM,changevalue,changevaluefn,changevalueop,check,check_conflict,' +
            'check_match,checkandincreasestack,checkname,checknext,chunk,close_func,closelistfield,clvalue,codearith,' +
            'codestring,cond,condassign,condfalse,condfalseassign,conditional_field,constructor,create_number,' +
            'createmetatable,cvalue,dataconstructor,dataitem,datarecord,datasimpleexp,defbody,duplicatestat,enterblock,' +
            'enterlevel,enumglobalstat,enumstat,error_expected,errorlimit,exchangestat,explist0,explist1,expr,' +
            'field,fnbody,fnchunk,fnparlist,freeexp,freeexps,funcargexplist,funcargexpr,funcargs,funcname,getbinopr,' +
            'getcode,getfrom,getunopr,hasmultret,hvalue,importstat,index2adr,index2value,indexupvalue,init_exp,' +
            'init_number,initlocals,isnext,item,l_isfalse,l_isfalseorfail,l_istrue,lastlistfield,leaveblock,leaveblocknumloop,' +
            'leavelevel,lisbdigit,lisodigit,listfield,listrange,lmemfind,localstat,luaC_barrier,luaC_barrierpair,' +
            'luaC_barrierreg,luaC_barrierseq,luaC_barrierset,luaC_barriert,luaC_checkGC,luaC_objbarrier,luaD_call,' +
            'luaD_checkstack,luaD_precall,luaD_rtableentry,luaE_freethread,luaE_newthread,luaE_warnerror,luaE_warning,' +
            'luaG_aritherror,luaG_checkcode,luaG_checkopenop,luaG_concaterror,luaG_errormsg,luaG_ordererror,luaG_runerror,' +
            'luaG_typeerror,luaG_typeerrorx,luaH_getstr,luaH_new,luaH_newkey,luaH_resizearray,luaH_setint,luaK_checkstack,' +
            'luaK_codeABC,luaK_codeABx,luaK_codeAsBx,luaK_concat,luaK_dischargevars,luaK_exp2RK,luaK_exp2anyreg,' +
            'luaK_exp2nextreg,luaK_exp2val,luaK_fixline,luaK_getlabel,luaK_goiffalse,luaK_goifnil,luaK_goiftrue,' +
            'luaK_ifoperation,luaK_indexed,luaK_infix,luaK_isnumeral,luaK_jump,luaK_nil,luaK_numberK,luaK_patchlist,' +
            'luaK_patchtohere,luaK_posfix,luaK_prefix,luaK_reserveregs,luaK_ret,luaK_self,luaK_setlist,luaK_setmultret,' +
            'luaK_setoneret,luaK_setreturns,luaK_storevar,luaK_stringK,luaL_Buffer,luaL_addchar,luaL_addlstring,' +
            'luaL_addsize,luaL_addstring,luaL_addvalue,luaL_argcheck,luaL_argerror,luaL_buffinit,luaL_callmeta,' +
            'luaL_checkany,luaL_checkcache,luaL_checkint,luaL_checkint32_t,luaL_checkinteger,luaL_checklstring,' +
            'luaL_checklstringornil,luaL_checknumber,luaL_checkoff64_t,luaL_checkoption,luaL_checksetting,luaL_checkstack,' +
            'luaL_checkstring,luaL_checktype,luaL_checkudata,luaL_checkuint32_t,luaL_clearbuffer,luaL_dofile,luaL_error,' +
            'luaL_execresult,luaL_fileresult,luaL_findtable,luaL_getmetafield,luaL_getmetatable,luaL_getn,luaL_getsubtable,' +
            'luaL_getudata,luaL_gsub,luaL_isstandardlib,luaL_isudata,luaL_len,luaL_libcfuncs,luaL_loadbuffer,luaL_loadfile,' +
            'luaL_loadstring,luaL_newmetatable,luaL_newstate,luaL_nonumorcmplx,luaL_openlibs,luaL_optint,luaL_optint32_t,' +
            'luaL_optinteger,luaL_optlong,luaL_optlstring,luaL_optnumber,luaL_optoff64_t,luaL_optstring,luaL_optunsigned,' +
            'luaL_prepbuffer,luaL_prepbuffsize,luaL_pushnexttemplate,luaL_pushresult,luaL_pushresultsize,luaL_ref,' +
            'luaL_register,luaL_requiref,luaL_setfuncs,luaL_setmetatable,luaL_setmetatype,luaL_setn,luaL_standardlibs,' +
            'luaL_testudata,luaL_tolstring,luaL_traceback,luaL_typecheck,luaL_typeerror,luaL_typename,luaL_typerror,' +
            'luaL_unref,luaL_where,luaM_growvector,luaO_arith,luaO_chunkid,luaO_fb2int,luaO_hexavalue,luaO_int2fb,' +
            'luaO_log2,luaO_numavalue,luaO_pushfstring,luaO_pushvfstring,luaO_rawarith,luaO_rawequalObj,' +
            'luaO_str2c,luaO_str2d,luaO_str2num,luaS_new,luaS_newlstr,luaT_typenames,luaV_approxequalvalonebyone,' +
            'luaV_arithmoperand,luaV_brotatesigned,luaV_brotateunsigned,luaV_concat,luaV_equalncomplex,luaV_equalref,' +
            'luaV_equalval,luaV_equalvalonebyone,luaV_execute,luaV_fastget,luaV_fastgeti,luaV_finishfastset,luaV_finishget,' +
            'luaV_finishset,luaV_gettable,luaV_lessequal,luaV_lessthan,luaV_objlen,luaV_settable,luaV_substring,' +
            'luaV_tocomplex,luaV_tonumber,luaV_tonumber_,luaV_tonumberx_,luaV_tostring,luaX_lexerror,luaX_lookahead,' +
            'luaX_newstring,luaX_next,luaX_syntaxerror,luaX_token2str,luaY_checklimit,luaY_parser,lua_absindex,' +
            'lua_arith,lua_arity,lua_assert,lua_atpanic,lua_call,lua_checkstack,lua_close,lua_closestate,lua_compare,' +
            'lua_concat,lua_copy,lua_cpcall,lua_createtable,lua_dump,lua_equal,lua_error,lua_gc,lua_getallocf,' +
            'lua_getarity,lua_getfenv,lua_getfenvi,lua_getfield,lua_getglobal,lua_geti,lua_getinfo,lua_getiuservalue,' +
            'lua_getlocal,lua_getmetatable,lua_getstack,lua_gettable,lua_gettop,lua_getupvalue,lua_getupvalues,' +
            'lua_getuservalue,lua_getwarnf,lua_hasfield,lua_insert,lua_isboolean,lua_iscfunction,lua_iscomplex,' +
            'lua_iseq,lua_isfail,lua_isfalse,lua_isfalseorfail,lua_isfunction,lua_isnil,lua_isnilfalseorfail,lua_isnone,' +
            'lua_isnoneornil,lua_isnumber,lua_ispair,lua_isreg,lua_isseq,lua_isset,lua_isstring,lua_istable,lua_isthread,' +
            'lua_istrue,lua_isuserdata,lua_isyieldable,lua_lessthan,lua_load,lua_lock,lua_log2,lua_newstate,lua_newtable,' +
            'lua_newthread,lua_newuserdata,lua_newuserdatauv,lua_next,lua_nupvalues,lua_objlen,lua_pcall,lua_pop,' +
            'lua_popen,lua_procattribs,lua_pushboolean,lua_pushcachevalue,lua_pushcclosure,lua_pushcfunction,lua_pushchar,' +
            'lua_pushcomplex,lua_pushfail,lua_pushfalse,lua_pushfstring,lua_pushinteger,lua_pushlightuserdata,' +
            'lua_pushliteral,lua_pushlstring,lua_pushnil,lua_pushnumber,lua_pushstring,lua_pushstringboolean,lua_pushthread,' +
            'lua_pushtrue,lua_pushundefined,lua_pushunsigned,lua_pushvalue,lua_pushvfstring,lua_rawaequal,lua_rawequal,' +
            'lua_rawget,lua_rawgetfield,lua_rawgeti,lua_rawgetp,lua_rawset,lua_rawset2,lua_rawseti,lua_rawsetiboolean,' +
            'lua_rawsetikey,lua_rawsetilstring,lua_rawsetinumber,lua_rawsetistring,lua_rawsetp,lua_rawsetstringboolean,' +
            'lua_rawsetstringchar,lua_rawsetstringinteger,lua_rawsetstringlong,lua_rawsetstringnumber,lua_rawsetstringpairnumbers,' +
            'lua_rawsetstringstring,lua_reginsert,lua_regnext,lua_remove,lua_replace,lua_resume,lua_rotate,lua_sdelete,' +
            'lua_seqinsert,lua_seqnext,lua_seqrawget,lua_seqrawget2,lua_seqrawgeti,lua_seqrawgetinumber,lua_seqrawseti,' +
            'lua_seqrawsetistring,lua_seqseticachevalue,lua_setallocf,lua_setfenv,lua_setfield,lua_setglobal,' +
            'lua_sethook,lua_setiuservalue,lua_setlocal,lua_setmetatable,lua_setmetatabletoobject,lua_settable,' +
            'lua_settop,lua_setupvalue,lua_setuservalue,lua_setwarnf,lua_sget,lua_shas,lua_sinsert,lua_srawget,' +
            'lua_srawset,lua_srawsetlstring,lua_srawsetnumber,lua_srawsetstring,lua_status,lua_strany2number,lua_stringtonumber,' +
            'lua_strlen,lua_strnext,lua_strx2number,lua_toboolean,lua_tocfunction,lua_toint32_t,lua_tointeger,' +
            'lua_tointegerx,lua_tolstring,lua_tonumber,lua_tonumberx,lua_tooff64_t,lua_topointer,lua_tostring,' +
            'lua_tothread,lua_tounsignedx,lua_touserdata,lua_type,lua_typename,lua_unlock,lua_upvalueindex,lua_usnext,' +
            'lua_warning,lua_writestringerror,lua_xmove,lua_yield,markupval,mneconstfolding,multiargop,multiargopmultrets,' +
            'new_localvar,new_localvarliteral,nvalue,open_func,opvmargexplist,pairvalue,parameter,parlist,plusplus,' +
            'prefixexp,primaryexp,procstat,pushclosure,pushnexttemplate,pvalue,rawuvalue,record,registerlocalvar,' +
            'regvalue,removevars,restorestack,retstat,returntype,rotatestat,savestack,searchvar,seqconstructor,' +
            'seqdataconstructor,seqdataitem,seqvalue,setbvalue,setclvalue,setconstructor,setfailvalue,sethvalue,' +
            'setivalue,setmetatable,setnilvalue,setnvalue,setobj,setobj2n,setobj2s,setobj2t,setobjs2s,setpairvalue,' +
            'setptvalue,setrealimagvalue,setregvalue,setseqvalue,setsvalue,setsvalue2s,setthvalue,setusertype,' +
            'setusvalue,simpleexp,singlevar,singlevaraux,singlevarexpr,stackdstat,str_checkname,str_getname,subexpr,' +
            'svalue,switchdstat,test_then_block,testnext,testnext2,thvalue,tsvalue,ttisboolean,ttiscomplex,ttisfail,' +
            'ttisfalse,ttisfunction,ttisint,ttisinteger,ttislightuserdata,ttisnegint,ttisnil,ttisnoneornil,ttisnonnegative,' +
            'ttisnonnegint,ttisnonposint,ttisnotnil,ttisnotnoneornil,ttisnumber,ttispair,ttisposint,ttisposint,' +
            'ttispositive,ttisreg,ttisseq,ttisstring,ttistable,ttisthread,ttistrue,ttisuserdata,ttisuset,ttype'
        end>
    end>
  P.FontTable = <
    item
      FontID = 0
      GlobalAttrID = 'Whitespace'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clBlack
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 1
      GlobalAttrID = 'Integer'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGreen
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 2
      GlobalAttrID = 'Float'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGreen
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 3
      GlobalAttrID = 'Integer'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGreen
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 4
      GlobalAttrID = 'Defines'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGreen
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 5
      GlobalAttrID = 'Comment'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGray
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsItalic]
    end
    item
      FontID = 6
      GlobalAttrID = 'Comment'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGray
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsItalic]
    end
    item
      FontID = 7
      GlobalAttrID = 'String'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clMaroon
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 8
      GlobalAttrID = 'logical'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clBlue
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 9
      GlobalAttrID = 'LuaFunctions'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clTeal
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 10
      GlobalAttrID = 'Reserved words'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clBlack
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 12
      GlobalAttrID = 'datatypes'
      BlockID = 1
      Font.Charset = DEFAULT_CHARSET
      Font.Color = ClMaroon
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 13
      GlobalAttrID = 'luadatatypes'
      BlockID = 1
      Font.Charset = DEFAULT_CHARSET
      Font.Color = ClNavy
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 11
      GlobalAttrID = 'Delimiters'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clBlack
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end>
  P.SyntaxVersion = 3
end