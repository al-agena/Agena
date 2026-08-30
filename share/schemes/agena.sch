object TPersHolder
  P.Name = 'Agena'
  P.SyntaxBlocks = <
    item
      Name = 'Default'
      ID = 0
      CaseSensitive = True
      UseMetaSymbol = True
      UseMetaToWrapLines = True
      FIText = 0
      FIIntNum = 1
      FIFloatNum = 2
      FIHexNum = 3
      FISymbol = 5
      UseSymbols = True
      UseComments = True
      UseMultiLineComments = True
      UseSingleLineComments = True
      UseStrings = True
      UseSingleLineStrings = True
      UseNumbers = True
      UseSuffixedNumbers = True
      UseMultipleNumSuffixes = False
      UseKeywords = True
      BlockDelimiters = <>
      SingleLineCommentDelimiters = <
        item
          FontID = 3
          LeftDelimiter = '#'
        end>
      MultiLineCommentDelimiters = <
        item
          FontID = 3
          LeftDelimiter = '#/'
          RightDelimiter = '/#'
        end
        item
          FontID = 3
          LeftDelimiter = '/*'
          RightDelimiter = '*/'
        end>
      SingleLineStringDelimiters = <
        item
          FontID = 4
          LeftDelimiter = #39
          RightDelimiter = #39
        end>
      IdentPrefixes = <
        item
          FontID = 4
          LeftDelimiter = '$'
        end>
      IdentPrefixesSuffixes = <
        item
          LeftDelimiter = '.'
        end>
      BlockDelimiters = <
        item
          LeftDelimiter = 'do'
          RightDelimiter = 'od'
          DelimitersArePartOfBlock = True
        end
        item
          LeftDelimiter = '<<'
          RightDelimiter = '>>'
          DelimitersArePartOfBlock = True
        end>
      KeywordSets = <
        item
          FontID = 5
          Name = 'keywords'
          Keywords =
            'alias,anything,as,basic,begin,boolean,bottom,break,by,case,catch,clear,cls,complex,constant,create,dec,def,delete,dict,div,do,downto,duplicate,' +
            'elif,else,end,enum,epocs,esac,esle,exchange,external,feature,fi,for,from,global,if,import,inc,insert,intdiv,integer,into,is,keys,' +
            'lightuserdata,listing,local,mne,mod,mul,muladd,nargs,negate,negative,next,nonnegative,nonnegint,nonzero,nonzeroint,nothing,number,' +
            'od,of,onsuccess,pair,pop,posint,positive,post,pre,proc,procedure,procname,quit,redo,reg,register,relaunch,reminisce,return,rotate,' +
            'scope,seq,sequence,set,skip,store,string,table,then,thread,to,top,try,unless,until,up,userdata,varargs,when,while,with,yrt,zero'
        end
        item
          FontID = 5
          Name = 'operators'
          Keywords =
            'abs,addup,antilog10,antilog2,arccos,arcsec,arcsin,arctan,assigned,atendof,bea,cbrt,char,cis,cos,cosh,cosxx,cube,empty,entier,even,exp,' +
            'filled,finite,flip,float,foreach,imag,in,infinite,instr,int,integral,intersect,invgamma,invsqrt,left,ln,lngamma,ltrim,minus,mulup,' +
            'nan,numeric,odd,qmdev,qsumup,real,recip,right,rtrim,sethigh,setlow,sign,signum,sin,sinc,sinh,size,split,sqrt,square,squareadd,sumup,surd,' +
            'tan,tanh,type,typeof,unassigned,union,unity'
        end
        item
          FontID = 6
          Name = 'logical'
          Keywords =
            'and,fail,false,nand,nor,not,notin,null,or,subset,true,xor,xsubset,' +
            '-:,::,<,<=,<>,=,>,>='
        end
        item
          FontID = 7
          Name = 'baselib'
          Keywords =
            'addtometatable,addup,alternate,append,approx,arccosh,arccot,arccoth,arccsc,arccsch,arcsinh,arcsech,arctan2,arctanh,argerror,argument,assume,aux,bessel0,bessel1,' +
            'besselj,bessely,beta,binsearch,binclude,binomial,bintersect,bisequal,bminus,cabs,cartesian,cas,cathet,ceil,checkoptions,checktype,cleanse,columns,concat,conjugate,copy,' +
            'copyadd,cosc,cot,coth,countitems,csc,csch,dblfact,degrees,descend,drop,drem,DoubleEps,duplicates,E,Eps,erf,erfc,erfcx,erfi,error,EulerGamma,eval,everyth,exp10,exp2,' +
            'expx2,fact,fma,floor,fold,frac,fractional,frexp,frexp10,gamma,getbit,getbits,getentry,getkey,getmeta,getmetatable,getnbits,gettype,globals,gtrap,has,heaviside,hEps,' +
            'hypot,hypot2,hypot3,identity,ilog10,ilog2,implies,include,infinity,initialise,intdiv,intersect,inverf,inverfc,invhypot,invpsi,invpytha,ipairs,iqmean,iqr,' +
            'iquo,irem,isall,isboolean,iscomplex,isequal,isint,isnegint,isnegative,isnonneg,isnonnegint,isnonposint,isnonzeroint,isnumber,isnumeric,ispair,isposint,ispositive,' +
            'isset,isseq,isstring,isstructure,istable,iterate,ldexp,lines,load,loadfile,loadstring,log,log10,log2,map,mdf,member,modf,modp,move,mprint,multiple,nand,' +
            'nextone,nor,op,ops,optboolean,optcomplex,optint,optnonnegint,optnonnegative,optnonzeroint,optnumber,optposint,optpositive,optstring,pack,pairs,pcall,Phi,Pi,Pi2,' +
            'PiO2,PiO4,pipeline,prepend,print,printf,proot,protect,psi,purge,put,pytha,pytha4,radians,rawget,rawgeti,rawset,rawseti,read,readlib,readlines,recurse,reduce,restart,roll,' +
            'root,round,rsorted,run,satisfy,save,sec,sech,select,selectremove,setbit,setbits,setmetatable,setnbits,settype,shift,sort,sorted,split,subs,subsop,swap,tanc,time,' +
            'times,tonumber,tonumberx,top,toseq,toset,tostring,tostringx,totable,trifact,undefined,unique,unpack,values,watch,whereis,write,writeline,xdf,xnor,zip'
        end
        item
          FontID = 7
          Name = 'metamethods'
          Keywords =
            '__abs,__absdiff,__acompare,__add,__aeq,__antilog10,__antilog2,__arccos,__arcsec,__arcsin,__arctan,__band,__bea,__bnot,__bor,__brotl,__brotr,__bshl,__bshr,__bxor,' +
            '__call,__cell,__cis,__compare,__concat,__conjugate,__cos,__cosh,__cosxx,__cube,__div,__eeq,__empty,__entier,__eq,__even,__exp,__filled,__finite,' +
            '__flip,__frac,__fractional,__gc,__imag,__in,__index,__infinite,__int,__intdiv,__integral,__intersect,__invsqrt,__ipow,__le,__left,__ln,__lngamma,' +
            ' __log,__lt,__meps,__minus,__mod,__mul,__mulup,__naeq,__nan,__nand,__negate,__nonzero,__nor,__notin,__odd,__oftype,__peps,__pow,__qmdev,__qsumup,__real,' +
            '__recip,__right,__sign,__signum,__sin,__sinc,__sinh,__size,__sqr,__square,__squareadd,__sub,__sumup,__tan,__tanh,__union,__unm,__weak,__writeindex,__zero'
        end
        item
          FontID = 7
          Name = 'strings'
          Keywords =
            'strings,a64,advance,align,appendmissing,between,bigrams,byte,c2f,capitalise,charmap,charset,chomp,chop,compare,contains,cut,' +
            'diamap,dice,diffs,dleven,dump,eq,f2c,fields,find,format,fuzzy,ge,glob,gmatch,gmatches,gseparate,gsub,gt,hextodec,hirschberg,hits,' +
            'isalpha,isalphanumeric,isalphaspace,isalphaspec,isascii,isblank,iscenumeric,iscomplex,isconsonant,iscontrol,isdia,isending,' +
            'isfractional,isgraph,ishex,isintegral,isisoalpha,isisolower,isisoprint,isisospace,isisoupper,islatin,islatinnumeric,' +
            'isloweralpha,islowerlatin,ismagic,ismultibyte,isnumber,isnumberspace,isnumeric,ispattern,isprintable,isspace,isspec,isstarting,' +
            'isupperalpha,isupperlatin,isutf8,isvowel,iswrapped,itouni,jaro,join,le,lcs,leven,ljustify,lower,lt,ltrim,match,matches,mfind,' +
            'neq,ngrams,obfusxor,pack,packsize,random,repeat,replace,reverse,rjustify,rotateleft,rotateright,rtrim,seek,separate,shannon,' +
            'strcoll,strcmp,strcspn,strftime,stricmp,strlen,strncmp,strnstr,strptime,strspn,strstr,strtoul,strverscmp,strxfrm,sub,subs,' +
            'tobytes,tochars,tolatin,toutf8,transform,trim,uncapitalise,unique,unpack,unwrap,upper,utf8size,walker,words,wrap,wrapmissing'
        end
        item
          FontID = 7
          Name = 'math'
          Keywords =
            'math,accu,agm,binet,bintodec,branch,ceillog2,ceilpow2,chi,chop,cld,clip,compose,convertbase,copysign,coscpi,cosd,cospi,cotd,cscd,dd,decompose,dirac,dms,' +
            'eps,epsilon,eq,expm1,exponent,expx2,fall,fdim,fld,flipsign,floorpow2,fmod,fpclassify,fraction,gammasign,ge,gt,hamming,hextodec,hgm,invlerp,isexceptional,' +
            'isminuszero,isnormal,isordered,ispow2,isqrt,issubnormal,kbadd,koadd,largest,lastcontint,le,leadzeros,length,lerp,lnabs,lnbeta,lnbinomial,lnfact,lnhypot,' +
            'lnp1,logs,lpad,lrs,lt,mantissa,max,min,modulus,morton,mulsign,nearmod,nearbyint,neq,nextafter,nextmultiple,nextpow2,nextpower,noise,norm,octtodec,onepinv,' +
            'piecewise,pochhammer,pow32,pow52,pow72,powe,prevmultiple,prevpow2,prevpower,ramp,random,randoms,randomseed,randomseeds,rectangular,rempio2,rint,redupi,rpad,' +
            'secd,signbit,significand,sinpi,sincos,sincospi,sincpi,sind,sinhcosh,smallest,smallestnormal,splitdms,tand,tanpi,tancpi,tocomplex,todecimal,todegrees,' +
            'tohex,toradians,tosgesim,triangular,trunc,two54,uexponent,ulp,unitise,unitstep,wrap,xlnp1,zeroab,zeroin,zerosubnormal'
        end
        item
          FontID = 7
          Name = 'numtheory'
          Keywords =
            'numtheory,binet,congruentprime,factors,fib,fibinv,gcd,ifactor,ifactors,invmod,iscoprime,iscube,isfib,' +
            'isprime,issqrfree,issquare,jacobi,kronecker,lcm,mulmod,nearmod,nextprime,nthpow,powmod,prevprime,primes'
        end
        item
          FontID = 7
          Name = 'bytes'
          Keywords =
            'bytes,add32,arshift32,bcd,bintogray,cast,castint,div32,divmod32,extract32,fpbtoint,getdouble,gethigh,getlow,getnumhigh,getnumlow,' +
            'getunbiased,getwords,graytobin,interweave,inttofpb,isint32,leadzeros,leastsigbit,mask32,mod32,mostsigbit,mul32,muladd32,mulmod32,' +
            'nand32,nextbit,nor32,not32,numhigh,numlow,numto32,numwords,onebits,optbytes,or32,pack,packsize,parity32,parity8,peek,powmod32,' +
            'replace32,reverse,rotate32,setdouble,sethigh,setlow,setnumhigh,setnumlow,setnumwords,setwords,shift32,sub32,swap,swaplower,' +
            'swapupper,tobinary,tobig,tobytes,tolittle,tonumber,trailzeros,unpack,xnor32'
        end
        item
          FontID = 7
          Name = 'fastmath'
          Keywords =
            'fastmath,cosfast,erf,floor,hypotfast,invroot,invsqrt,lbfast,reciprocal,sincosfast,sinfast,sqroot,sqrtfast,tanfast'
        end
        item
          FontID = 7
          Name = 'binio'
          Keywords =
            'binio,clearerror,close,eof,ferror,fgetpos,filepos,fsetpos,getieee,getieeedouble,getieeeexpo,getieeemahi,getieeemalo,getieeesign,' +
            'ieee,isatty,isfdesc,length,lines,open,readbytes,readchar,readindex,readint64,readlong,readlongdouble,readnumber,readobject,readshortstring,' +
            'readstring,readuint64,rewind,seek,setieee,setieeedouble,setieeeexpo,setieeemahi,setieeemalo,setieeesign,sync,toend,writebytes,writechar,' +
            'writedouble,writeindex,writeint64,writeline,writelong,writelongdouble,writenumber,writeobject,writeshortstring,writestring,writeuint64'
        end
        item
          FontID = 7
          Name = 'calc'
          Keywords =
            'calc,Ai,arclen,auxSiCi,Bi,brent,chandrupatla,cheby,cheby64,chebycoeffs,chebygen,chebyt,Chi,Ci,Cin,clampedspline,clampedsplinecoeffs,' +
            'curvature,dawson,dct,diff,differ,dilog,dst,Ei,Ein,elliptic1,elliptic2,En,eps,eta,eucliddist,eulerdiff,expn,extrema,' +
            'fmaxbr,fmaxgs,fminbr,fmings,fresnelc,fresnels,fsum,gammainc,gauleg,gauleg64,gaussian,gd,gtrap64,harmomic,horner,hyp1f1,hyp2f1,' +
            'ibeta,igamma,igammac,inflect,intcc,intcc64,intde,intde64,intdei,intdei64,intdeo,intdeo64,integ,interp,invibeta,iscont,isdiff,itp,' +
            'jacobian,lambda,lambertw,Li,limit,linterp,logistic,logit,maximum,mean,mean64,minimum,nakspline,naksplinecoeffs,neville,' +
            'newtoncoeffs,poles,polyfit,polygen,polylog,probit,Psi,regulafalsi,riesum,riesum64,saddles,savgol,savgolcoeffs,scaleddawson,sections,' +
            'Shi,Si,SiCi,sigmoid,simaptive,simaptive64,sinuosity,smoothstep,softsign,Ssi,variance,w,weier,xpdiff,zeta,zeroin,zeros'
        end
        item
          FontID = 7
          Name = 'linalg'
          Keywords =
            'linalg,add,addcol,addrow,adjoint,antidiagonal,augment,backsub,checkmatrix,checksquare,checkvector,col,coldim,colvector,copyinto,' +
            'countitems,crossprod,delcols,delrows,det,diagonal,dim,dotprod,eigen,eigenval,extend,fib,forsub,gausselim,gaussjord,' +
            'getantidiagonal,getdiagonal,getvelem,hilbert,identity,infcolnorm,infnorm,innerprod,inverse,isantidiagonal,isantisymmetric,' +
            'isdiagonal,isfractional,isidentity,isintegral,islower,ismatrix,isone,isref,isrref,issingular,issparse,issquare,issymmetric,' +
            'isupper,isvector,iszero,kronprod,linsolve,ludecomp,ludoolittle,matinfnorm,matmat,matnnorm,matonenorm,matrix,mattam,' +
            'mcopy,meq,minor,mmap,mmul,mpow,mulrow,mulrowadd,multiply,mzip,ncolnorm,newmatrix,nnorm,norm,onecolnorm,onenorm,ones,' +
            'outprodmatrix,permanent,pivot,randmatrix,randvector,rank,reshape,romberg,rotcol,rotrow,row,rowdim,rref,scalardiv,scalarmul,' +
            'scale,setvelem,sparse,stack,sub,submatrix,subvector,swapcol,swaprow,totable,trace,transpose,unitvector,vcopy,vectdim,vector,veq,' +
            'viszero,vmap,vzero,vzip,zeros'
        end
        item
          FontID = 7
          Name = 'stats'
          Keywords =
            'stats,accu,acf,acv,ad,amean,besselj,besselk,beta,binomd,binompdf,brownian,card,cauchy,cdf,cdfnormald,chauvenet,checkcoordinate,chisquare,' +
            'circular,colnorm,constant,countentries,covar,cubic,cumsum,dampedcos,dampedsin,dbscan,deltalist,durbinwatson,ema,exponential,extrema,' +
            'F,Fc,fivenum,fprod,fratio,fraqd,freqd,fsum,gammad,gammadc,gammacdf,gammapdf,gaussian,gema,geometric,gini,gmean,gsma,gsmm,' +
            'herfindahl,hmean,hole,hypergeom,invbinomd,invF,invnormald,ios,iqr,isall,isany,issorted,kurtosis,laplace,linear,logistic,lognormald,' +
            'logseries,lse,mad,matern,max,md,mean,meanmed,meanqmdev,meanvar,median,min,minmax,mode,moment,nde,ndf,negbinompdf,neighbours,normald,' +
            'obcount,obpart,pdf,peaks,penta,percentile,poisson,poissond,power,prange,probit,qcd,qmean,quartiles,ratquad,rownorm,scale,sd,sdko,' +
            'skewness,sma,smallest,smm,spherical,spread,ssd,standardise,studentst,sumdata,sumdataln,tovals,trimean,trimmean,var,weights,' +
            'white,winsor,zscore'
        end
        item
          FontID = 7
          Name = 'combinat'
          Keywords =
            'combinat,bell,bernoulli,catalan,cartprod,choose,euler,fib,numbcomb,numbpart,numbperm,permute,stirling1,stirling2'
        end
        item
          FontID = 7
          Name = 'fractals'
          Keywords =
            'fractals,checkcolourmap,draw,esctime,genmap,lambdafn,lbea,lcos,lcosxx,lsin,mandel,mandelbrot,mandelbrotfast,markmand,newton,' +
            'paint,readmap,rgb2hsl,sortmap,writemap'
        end
        item
          FontID = 7
          Name = 'package'
          Keywords =
            'package,checkclib,getcfuncs,loadclib,loaded,packages,readlibbed'
        end
        item
          FontID = 7
          Name = 'sequences'
          Keywords =
            'sequences,col,dimension,extend,getdim,isall,isrectangular,issquare,isone,iszero,move,new,newseq,numunion,' +
            'numintersect,numminus,resize,seqofseqs,subs'
        end
        item
          FontID = 7
          Name = 'sets'
          Keywords =
            'sets,cleanse,isall,new,newset,numunion,numintersect,numminus,resize'
        end
        item
          FontID = 7
          Name = 'registers'
          Keywords =
            'registers,col,extend,getdim,gettop,isall,isone,isrectangular,issquare,iszero,move,new,newreg,numintersect,numminus,' +
            'numunion,reduce,regofregs,settop,subs'
        end
        item
          FontID = 7
          Name = 'rtable'
          Keywords =
            'defaults,forget,get,init,mode,purge,put,remember,roinit,rtable'
        end
        item
          FontID = 10
          Name = 'phonetiQs'
          Keywords =
            'phonetiqs,anorm,cantor,comp,compare,config,cv,cvCore,cvLike,isFirstName,isLastName,isTypo,makeregex,' +
            'numbernorm,numnorm,pCore,phones,phonesCore,phq,pLike,pnorm,qnorm,rawphonesCore,readDict,readNames,' +
            'searchDict,singlesubs,toGenders,typo,val,writeDict,writeNames'
        end
        item
          FontID = 11
          Name = 'phonetiQs_tables'
          Keywords =
            'baseinfo,boys,cmplx_mt,cvPhonemes,delsymbols_substable,girls,initstring,loaded,openfiles,' +
            'similarVowels,SimplePhoneticMapping,substCV,surnames,typoKeys,validsurnames'
        end
        item
          FontID = 7
          Name = 'ads'
          Keywords =
            'ads,attrib,closebase,closeall,clean,comment,createbase,createseq,createdict,desc,' +
            'expand,fastclose,fastopen,fastread,fastseek,filepos,free,getall,getkeys,getvalues,' +
            'index,indices,invalids,iterate,lookup,openbase,opendict,peekin,' +
            'rawsearch,readbase,remove,retrieve,search,seqtolist,setdesc,sizeof,sync,writebase'
        end
        item
          FontID = 7
          Name = 'compress'
          Keywords =
            'compress,attrib,close,lines,open,ratio,read,stringtoset'
        end
        item
          FontID = 7
          Name = 'utils'
          Keywords =
            'utils,calendar,celsius,decodeb32,decodeb64,decodeb85,decoderawxml,decodexml,decodexml_convert,decodez85,encodeb32,encodeb64,' +
            'encodeb85,encodexml,encodez85,fahren,findfiles,foot,hexlify,ilog2,isdate,km,meter,metre,mile,multidim,newsize,numiters,' +
            'onedim,parseargs,posrelat,radini,readb64,readcsv,readxml,rfc3339,singlesubs,timestamp,udata,unhexlify,uuid,weekday,writeb64,' +
            'writecsv,writeini,writexml'
        end
        item
          FontID = 7
          Name = 'units'
          Keywords =
            'units,celsius,cm,fahren,floz,foot,gallon,gram,inch,km,litre,liter,meter,metre,mile,ounce,yard'
        end
        item
          FontID = 7
          Name = 'io'
          Keywords =
            'io,anykey,clearerror,close,eof,ferror,filepos,filesize,getclip,getkey,infile,input,isfdesc,isopen,lines,lock,' +
            'maxopenfiles,mkstemp,move,nlines,open,output,pcall,pclose,popen,putclip,read,readfile,readfrom,readlines,readto,rewind,' +
            'seek,setvbuf,skiplines,sync,terminate,tmpfile,toend,truncate,unlock,write,writefile,writeline'
        end
        item
          FontID = 7
          Name = 'tables'
          Keywords =
            'tables,allocate,array,auto,borders,col,concat,dimension,entries,extend,getarray,getdim,getfield,gethash,geti,getsize,getsizes,hash,' +
            'hashole,indices,isall,isarray,ishash,isone,isrectangular,issquare,isnullarray,iszero,maxn,move,new,newtable,numintersect,' +
            'numminus,numunion,pack,parts,remove,reshuffle,resize,seqt,setfield,settable,subs,tableoftables,unpack'
        end
        item
          FontID = 7
          Name = 'gdi'
          Keywords =
            'gdi,arc,arcfilled,autoflush,background,checkoptions,circle,circlefilled,COLOURS,ellipse,ellipsefilled,flush,' +
            'ink,line,lineplot,mouse,mouseflush,mousestate,newline,options,plot,plotfn,plotfn2d,point,pointplot,rectangle,' +
            'rectanglefilled,setarc,setarcfilled,setcircle,setcirclefilled,setellipse,setellipsefilled,setinfo,setline,' +
            'setoptions,setpoint,setrectangle,setrectanglefilled,setthickness,settriangle,settrianglefilled,structure,' +
            'thickness,triangle,trianglefilled'
        end
        item
          FontID = 7
          Name = 'os'
          Keywords =
            'os,battery,beep,bios,cachelinesize,cdrom,chdir,chmod,chown,clock,codepage,computername,countcore,cpuid,cpuinfo,cpuload,curdir,curdrive,date,' +
            'datetosecs,difftime,dirname,drives,drivestat,endian,environ,esd,execute,exists,exit,faccess,fattrib,fcopy,filename,freemem,' +
            'fstat,ftok,getadapter,getcommandline,getconsolemode,getdirpathsep,getextlibpath,getenv,getfullpathname,getip,getlanguage,getloadeddlls,getlocale,getmac,' +
            'getmodulefilename,getprocesses,gettemppath,getthreadid,getthreadseed,getusbdevices,getwinsysdirs,groupinfo,hasnetwork,inode,isamd,isansi,isarm,' +
            'isarm32,isarm64,iscyrix,isdir,isdocked,isdos,isdriveletter,isdst,isfile,isintel,islinux,islinux386,islocale,ismac,ismounted,' +
            'isnexgen,isnsc,isos2,isppc,isremovable,issolaris,issysdir,isunix,isvaliddrive,isvalidpath,isvia,iswindows,isx86,isdow,iterate,' +
            'list,listcore,login,lsd,lscore,md,meminfo,memstate,mkdir,monitor,mouse,move,netdomain,netsend,netuse,now,pause,period,pid,' +
            'ping,prefix,printcomplex,printenclosestrings,printlongtable,printpair,printregister,printsequence,printset,printtable,pwd,rd,' +
            'readlink,realpath,rename,rmdir,secstodate,setconsolemode,setenv,setextlibpath,setlocale,settime,shellcolour,shellgeom,' +
            'shellinfo,speed,strerror,suffix,symlink,system,ticker,time,timestamp,tmpdir,tmpname,tzdiff,unmount,uptime,usd,userinfo,vga,' +
            'wait,whereis,winver,xgetbv'
        end
        item
          FontID = 7
          Name = 'clock'
          Keywords =
            'clock,add,adjust,clock_mt,muls,pow,sgstr,sub,tm,todec,totm'
        end
        item
          FontID = 7
          Name = 'environ'
          Keywords =
            'environ,anames,arity,arithstate,attrib,aux,callable,cpu,decpoint,frozen,gc,gdidefaultoptions,getconstants,getfenv,getopt,' +
            'homedir,infolevel,isafunc,iscfunc,isequal,isselfref,kernel,libpatchlevel,maxnumber,maxpathlength,minnumber,more,onexit,os,' +
            'pointer,ref,release,setfenv,umaxlong,unref,used,userinfo,warn,withprotected,withverbose'
        end
        item
          FontID = 7
          Name = 'bags'
          Keywords =
            'bags,attrib,bagtoset,getsize,include,minclude,new,remove'
        end
        item
          FontID = 7
          Name = 'lookup'
          Keywords =
            'lookup,checktable,getsizes,gettable,include,indices,istable,iterate,map,new,next,purge,setsizes,subs'
        end
        item
          FontID = 7
          Name = 'bimaps'
          Keywords =
            'bimaps,attrib,bimap,countitems,entries,indices,map,rawget,remove,select,subs,subsop'
        end
        item
          FontID = 7
          Name = 'lifo'
          Keywords =
            'lifo,attrib,getstore,has,include,new,peek,remove,shrink'
        end
        item
          FontID = 7
          Name = 'fifo'
          Keywords =
            'fifo,attrib,getstore,has,include,new,peek,remove,shrink'
        end
        item
          FontID = 7
          Name = 'trie'
          Keywords =
            'trie,attrib,find,getstore,has,include,new,remove'
        end
        item
          FontID = 7
          Name = 'heaps'
          Keywords =
            'heaps,attrib,avl,binary,entries,explore,find,get,getmax,getmin,getminmax,getroot,getstore,height,' +
            'include,indices,iterate,new,remove,reorder,skew'
        end
        item
          FontID = 7
          Name = 'rbtree'
          Keywords =
            'rbtree,entries,include,iterate,max,min,minmax,new,remove'
        end
        item
          FontID = 7
          Name = 'llist'
          Keywords =
            'llist,append,checkdlist,checkllist,dlist,dump,getitem,iterate,new,prepend,purge,put,replicate,setitem,toseq,totable'
        end
        item
          FontID = 7
          Name = 'ulist'
          Keywords =
            'ulist,append,checkulist,dump,getitem,getllist,getsize,has,isulist,iterate,new,prepend,purge,put,' +
            'setitem,sort,swap,toseq,tostring,totable'
        end
        item
          FontID = 7
          Name = 'xml'
          Keywords =
            'xml,close,decode,decodexml,getbase,getcallbacks,new,parse,pos,readxml,setbase,setencoding'
        end
        item
          FontID = 7
          Name = 'json'
          Keywords =
            'json,decode,encode,nil'
        end
        item
          FontID = 7
          Name = 'ini'
          Keywords =
            'ini,attrib,close,dump,getitem,getsection,hash,new,read,setitem,unset'
        end
        item
          FontID = 7
          Name = 'regex'
          Keywords =
            'regex,count,flags,find,match,new'
        end
        item
          FontID = 7
          Name = 'skycrane'
          Keywords =
            'skycrane,bagtable,dice,fcopy,formatline,fpbtoint,getlocales,inttofpb,isemail,iterate,move,obcount,readcsv,replaceinfile,' +
            'scribe,sorted,stopwatch,tee,tocomma,todate,tolerance,trimpath,xmlmatch'
        end
        item
          FontID = 7
          Name = 'factory'
          Keywords =
            'factory,anyof,count,curry,cycle,iterate,reset'
        end
        item
          FontID = 7
          Name = 'xbase'
          Keywords =
            'xbase,attrib,close,eof,field,fields,fieldtype,filepos,header,ismarked,isopen,isvoid,kernel,lock,mark,new,open,purge,readdbf,' +
            'readvalue,record,records,sync,unlock,wipe,write,writeboolean,writebyte,writecomplex,writedate,writedecimal,writefloat,writelong,' +
            'writenumber,writeshort,writestring,writetime,writeushort'
        end
        item
          FontID = 7
          Name = 'astro'
          Keywords =
            'astro,a2af,a2tf,autumn,cal2jd,cdate,coords,cweek,cweekmonsun,d2tf,dectodms,dmstodec,gethourangle,gethourangle,hdate,hebrew2jd,' +
            'illumination,isdst,isleapyear,isvaliddate,isvalidtime,jdate,lastcweek,locate,lunareclipse,lunarlibration,moon,moonphase,' +
            'moonriseset,pdate,peak,persian2jd,pmonth,searchaltitude,searchhourangle,searchlunarapsis,searchmoonphase,searchplanetapsis,' +
            'searchriseset,seasons,shabbat,siderial,solareclipse,spring,summer,sun,sunriseset,taiutc,tf2d,winter'
        end
        item
          FontID = 7
          Name = 'net'
          Keywords =
            'net,accept,address,bind,block,close,closewinsock,connect,getaddrinfo,gethostname,htonl,htons,isconnected,isipv4,isipv6,' +
            'keep,listen,lookup,ntohl,ntohs,open,opensockets,openwinsock,receive,remoteaddress,send,smallping,shutdown,survey,' +
            'tohostname,toip,wget'
        end
        item
          FontID = 7
          Name = 'divs'
          Keywords =
            'divs,acosine,add,asine,atangent,cosine,denom,divide,equals,expe,hcosine,' +
            'hsine,htangent,ipow,loge,multiply,numer,sine,sqroot,subtract,tangent,todec,todiv'
        end
        item
          FontID = 7
          Name = 'text'
          Keywords =
            'text,hyphenate,regex'
        end
        item
          FontID = 7
          Name = 'tar'
          Keywords =
            'tar,close,extract,lines,open'
        end
        item
          FontID = 7
          Name = 'numarray'
          Keywords =
            'numarray,append,attrib,binsearch,cdouble,celsius,checkarray,convert,countitems,cycle,double,fahren,find,floz,gallon,getbit,geti,' +
            'getitem,getparts,getsize,gram,include,int32,int64,introsort,isall,isarray,iterate,km,litre,longdouble,map,member,mile,' +
            'minmax,new,one,ounce,prepend,purge,read,readcdoubles,readdoubles,readint64,readintegers,readlongdoubles,readuchars,' +
            'readuint32,readuint64,readushorts,redim,remove,replicate,resize,satisfy,select,setbit,seti,setitem,setparts,sort,sorted,' +
            'subarray,subs,toarray,toreg,toseq,totable,uchar,uint32,uint64,unique,used,ushort,whereis,write,xadd,xantilog2,xarccos,' +
            'xarcsin,xarctan,xcos,xcosh,xdiv,xexp,xln,xlog2,xmul,xrecip,xsin,xsinh,xsquare,xsqrt,xsub,xtan,xtanh,zip'
        end
        item
          FontID = 7
          Name = 'debug'
          Keywords =
            'debug,features,funcname,getfenv,gethook,getinfo,getk,getlocal,getlocals,getmetatable,' +
            'getregistry,getrtable,getstore,getupvalue,getupvalues,globals,listcode,nupvalues,setfenv,' +
            'sethook,setlocal,setmetatable,setstore,setupvalue,system,traceback'
        end
        item
          FontID = 7
          Name = 'registry'
          Keywords =
            'registry,anchor,anyid,get,ref,unref'
        end
        item
          FontID = 7
          Name = 'zx'
          Keywords =
            'zx,ABS,ACS,ACSH,ADD,AND,ASN,ASNH,ATN,ATNH,COS,COSH,COT,COTH,CSC,CSCH,DIV,ERF,ERFC,EXP,GAM,HYP,INT,LGAM,LN,MOD,MUL,NOT,OR,POW,' +
            'SEC,SECH,SGN,SIG,SIN,SINH,SQR,SUB,TAN,TANH,E,PI,genseries,getcoeffs,reduce,setcoeffs'
        end
        item
          FontID = 7
          Name = 'stack'
          Keywords =
            'stack,absd,absindex,acosd,addtod,addtwod,allotted,antilogd,arccoshd,arccosd,arcsinhd,arcsind,arctan2d,arctanhd,arctand,attribd,' +
            'cathetd,cbrtd,cell,choosed,coshd,cosd,cotd,cscd,dequeued,divtwod,dumpd,enqueued,erfd,exp10d,exp2d,expd,explored,fmad,fracd,hypotd,' +
            'insertd,intd,intdivd,intdivtwod,invhypotd,isempty,lnd,logd,mapd,meand,modd,modtwod,mulbyd,multwod,mulupd,negated,' +
            'popd,powd,powtwod,pushd,pushstringd,pushvalued,pythad,readbytes,recipd,removed,replaced,resetd,reversed,rootd,rotated,' +
            'secd,selected,shrinkd,sinhd,sind,sized,sorted,sqrtd,squared,subtwod,sumupd,swapd,switchd,switchto,tanhd,tand,writebytes'
        end
        item
          FontID = 7
          Name = 'gzip'
          Keywords =
            'gzip,close,deflate,inflate,lines,open,read,seek,sync,write'
        end
        item
          FontID = 7
          Name = 'keyfile'
          Keywords =
            'keyfile,allkeys,close,commit,has,isopen,iterate,new,open,purge,read,start,sync,update,write'
        end
        item
          FontID = 7
          Name = 'minizip'
          Keywords =
            'minizip,add,addfile,addfrom,adler32,attribs,close,compress,crc32,decompress,extract,finalise,index,isdir,' +
            'iszip,num,offset,open,rawfd,read,stat,write'
        end
        item
          FontID = 7
          Name = 'bloom'
          Keywords =
            'bloom,attrib,find,get,include,new,toseq'
        end
        item
          FontID = 7
          Name = 'cuckoo'
          Keywords =
            'cuckoo,attrib,find,include,new,remove'
        end
        item
          FontID = 7
          Name = 'numcuckoo'
          Keywords =
            'numcuckoo,attrib,find,hash,include,new,remove'
        end
        item
          FontID = 7
          Name = 'numfilter'
          Keywords =
            'numfilter,attrib,find,hash,include,new'
        end
        item
          FontID = 7
          Name = 'dblhash'
          Keywords =
            'dblhash,attrib,compact,getindex,getitem,has,hash,include,iterate,new,purge,toseq,totable'
        end
        item
          FontID = 7
          Name = 'suffix'
          Keywords =
            'suffix,autocomplete,count,find,findall,gfind,lcp,read,sa,save,unique'
        end
        item
          FontID = 7
          Name = 'sema'
          Keywords =
            'sema,close,isopen,limit,new,open,reset,shrink,state'
        end
        item
          FontID = 7
          Name = 'utf8'
          Keywords =
            'utf8,chars,charpattern,codes,codepoint,len,offset'
        end
        item
          FontID = 7
          Name = 'memfile'
          Keywords =
            'memfile,append,attrib,bitfield,bytebuf,charbuf,clearbit,dump,find,getbit,getbyte,getbytes,getchar,getfield,' +
            'getitem,getsize,iterate,map,match,mfind,move,prepend,purge,put,read,replicate,replace,resize,' +
            'reverse,rewind,setbit,setbyte,setbytes,setchar,setfield,setitem,shift,substring,tostring,write'
        end
        item
          FontID = 7
          Name = 'bfield'
          Keywords =
            'bfield,clearbit,flipbit,getbit,getbyte,new,resize,setbit,setbitto,setbyte'
        end
        item
          FontID = 7
          Name = 'tuples'
          Keywords =
            'tuples,getitem,getsize,isall,iterate,map,member,new,remove,select,setitem,subs,toreg,toseq,tostring,totable,unpack'
        end
        item
          FontID = 7
          Name = 'dual'
          Keywords =
            'dual,arccot,arccoth,arccsc,arccsch,arccosh,arcsech,arcsinh,arctan,arctan2,arctanh,cathet,beta,cas,cbrt,const,cosc,' +
            'cot,coth,csc,csch,diff,erf,erfc,erfcx,exp10,exp2,expm1,expx2,gamma,generate,hypot,inverf,inverfc,invhypot,isdual,iseight,' +
            'ishyper,lnp1,log10,log2,pow32,pow52,pow72,powe,psi,pytha,pytha4,sec,sech,tanc,totable,tostring'
        end
        item
          FontID = 7
          Name = 'mapm'
          Keywords =
            'mapm,approx,bprep,carccosh,carcsinh,carctan2,carctanh,cargument,ccbrt,ccosc,ccot,ccoth,ccsc,ccsch,cexp10,cexp2,cfma,checkcnumber,' +
            'checkinteger,checknonnegative,checknonnegint,checkposint,checkpositive,checkxnumber,clog10,clog2,cnumber,csec,csech,csinc,' +
            'csincos,csinhcosh,ctanc,ctocomplex,ctonumber,ctostring,EulerGamma,InvPi,iscnumber,isxnumber,max,min,minmax,new,psi,' +
            'swap,tonumber,trim,xabs,xadd,xarccos,xarccosh,xarcsec,xarcsin,xarcsinh,xarctan,xarctan2,xarctanh,xband,xbnot,xbnor,xbor,' +
            'xbshl,xbshr,xbxor,xcathet,xcbrt,xceil,xchebyt,xcompare,xcos,xcosc,xcosh,xcot,xcoth,xcsc,xcsch,xcube,xdigits,xdigitsin,xdiv,xexp,xexp10,' +
            'xexp2,xexponent,xfactorial,xfma,xfloor,xfrac,xgamma,xgcd,xhypot,xidiv,xint,xintfrac,xinv,xinvmod,xiseven,xisint,xisnegint,' +
            'xisnegative,xisnonnegint,xisnonnegative,xisodd,xisposint,xispositive,xisprime,xispow2,xlcm,xln,xlog,xlog10,xlog2,xmax,xmin,' +
            'xmod,xmul,xmulmod,xneg,xnumber,xpow,xpowmod,xpsi,xrandom,xrandomseed,xround,xsec,xsech,xsign,xsin,xsinc,xsincos,xsinh,' +
            'xsinhcosh,xsqrt,xsquare,xsub,xtan,xtanc,xtanh,xterm,xtostring,xtonumber,xerf,xerfc,Zeta2'
        end
        item
          FontID = 7
          Name = 'long'
          Keywords =
            'long,approx,arccos,arccosh,arccot,arccoth,arccsc,arccsch,arcsec,arcsech,arcsin,arcsinh,arctan,arctanh,arccosh,arccoth,arcsech,beta,' +
            'besselj,bessely,binomd,cas,cathet,cbrt,ceil,checklong,chop,copysign,cos,cosh,cot,coth,count,csc,csch,eight,eighth,eleven,eps,erf,' +
            'erfc,erfcx,EulerGamma,expm1,exponent,expx2,fact,fifth,fifty,five,floor,fma,fmax,fmin,fmod,four,fpclassify,fraction,frexp,' +
            'gamma,gsolve,half,hundred,hundredth,hypot,hypot2,hypot3,infinity,invbinomd,inverf,inverfc,Invln2,InvlnPhi,InvPhi,' +
            'InvPhiSq,InvPi,InvPi2,InvPiO4,InvPiSqO4,Invsqrt2,isdual,isirregular,isequal,isfinite,isinfinite,isless,islessequal,isnegint,' +
            'isnegative,isnonnegint,isnormal,isposint,issubnormal,isundefined,isunequal,koadd,ldblmax,ldblmin,ldblepsilon,ldexp,ln2,lnabs,' +
            'lnfact,lnp1,lnPhi,log10,log2,long.ilog2,mantissa,modf,multiple,naught,nextafter,nine,norm,normalise,nought,one,overflow,' +
            'Phi,Pi,Pi2,PiO180,PiO2,PiO4,polygen,psi,pytha,pytha4,quarter,redupi,rempio2,root,round,scalb,sec,sech,seven,seventeenth,' +
            'signbit,significand,sin,sincos,sinh,sinhcosh,six,sixth,sixteenth,sqrt,sqrt2,sqrt3,tan,tanh,ten,tenth,third,thousand,' +
            'thousandth,three,threequarter,tonumber,tostring,twelfth,twelve,two,unm,wrap,xlngamma,zeroin,zerosubnormal,Zeta2'
        end
        item
          FontID = 7
          Name = 'ints'
          Keywords =
            'ints,cbrt,checkint64,copy,eleven,fifty,five,four,gcd,getbit,getbits,gethigh,getlow,getwords,int64,invmod,iscoprime,isint64,' +
            'isnegint,isnonnegint,isposint,ispow2,isprime,lcm,leadzeros,leastsigbit,log10,log2,max,mean,min,minmax,mostsigbit,mulmod,' +
            'naught,nextpow2,nextprime,nought,one,onebits,parity,powmod,prevprime,random,reverse,root,setbit,sethigh,setlow,setwords,ten,' +
            'three,tobytes,todouble,toint64,tonumber,tostring,touint64,trailzeros,two,uint64'
        end
        item
          FontID = 7
          Name = 'gmp'
          Keywords =
            'gmp,add,addmul,argument,attrib,band,binomial,bor,bxor,clrbit,cmp,cmpabs,com,combit,divide,factorial,fib,gcd,' +
            'gcdext,getbit,getd2exp,hamdist,invert,invm,iseven,isodd,ispow2,jacobi,kronecker,lcm,leastsigbit,' +
            'legendre,log10,log2,lucas,max,min,minmax,modulus,mostsigbit,mul2exp,mulm,multiply,neg,nextprime,' +
            'perfectpower,perfectsquare,popcount,power,powm,prevprime,primorial,remove,root,rootrem,scan0,scan1,' +
            'setbit,sint,sizeinbase,submul,subtract,swap,tdiv,tdivq,tdivr,testprime,tonumber,tostring,uint'
        end
        item
          FontID = 7
          Name = 'mpfr'
          Keywords =
            'mpfr,add,agm,ai,approx,arccosh,arccoth,arccsc,arccsch,arcsech,arcsinh,arctan2,arctanh,argument,beta,cathet,cbrt,ceil,checkcmpf,' +
            'checkmpf,clone,cmp,cmpd,copysign,cot,coth,csc,csch,digamma,dim,div2exp,divide,eint,erf,erfc,exp10,exp2,floor,fma,fmod,' +
            'fms,frexp,gamma,getparts,hypot,Inf,isfinite,isfractional,isinfinite,isintegral,isnonzero,isodd,iseven,isundefined,' +
            'iszero,iscmpf,ismpf,j0,j1,jn,lgamma,li2,log10,log2,max,min,modf,mul2exp,multiply,Nan,new,nexttoward,polygen,pow,' +
            'precision,pytha,pytha4,randinit,random,recsqrt,relerror,remquo,root,round,rounding,sec,sech,signbit,subtract,swap,' +
            'tonumber,tostring,trunc,y0,y1,yn,zeta,Zero'
        end
        item
          FontID = 7
          Name = 'kiss'
          Keywords =
            'kiss,fft,nextsize'
        end
        item
          FontID = 7
          Name = 'unimath'
          Keywords =
            'unimath,approx,det,fma,linsolve,max,min,sections,xpdiff,zeroin,zeros'
        end
        item
          FontID = 7
          Name = 'cordic'
          Keywords =
            'cordic,carccos,carcsin,carctan2,carctanh,ccbrt,ccos,ccosh,cexp,chypot,cln,cmul,cpow,csin,csinh,csqrt,ctan,ctanh'
        end
        item
          FontID = 7
          Name = 'dd'
          Keywords =
            'dd,arccsc,arccot,arctan2,cbrt,ceil,checkdd,cot,coth,csc,csch,copysign,erf,erfc,fact,floor,fma,frexp,get,hypot,hypot4,' +
            'isdd,ldexp,log2,log10,modf,new,renorm,root,round,sec,sech,signbit,sincos,tonumber,tostring,trunc'
        end
        item
          FontID = 7
          Name = 'curses'
          Keywords =
            'curses,addch,addchstr,addstr,attroff,attron,attrset,baudrate,beep,border,box,cbreak,cleanse,clearok,clone,close,clrtobot,' +
            'clrtoeol,color_pair,color_pairs,colors,cols,copywin,curs_set,cursyncup,delay_output,delch,deleteln,derive,' +
            'doupdate,echo,echoch,endwin,erase,erasechar,flash,flushinp,getbegyx,getbkgd,getch,getmaxyx,getparyx,getstr,getyx,' +
            'halfdelay,has_colors,has_ic,has_il,hline,idcok,idlok,immedok,init_pair,insertln,intrflush,is_linetouched,' +
            'is_wintouched,isendwin,keyname,keypad,killchar,leaveok,lines,longname,meta,move,move_derived,move_window,mvaddch,' +
            'mvaddchstr,mvaddstr,mvdelch,mvgetch,mvgetstr,mvhline,mvwinch,mvwinchnstr,mvwinnstr,mvwinsch,mvwinsnstr,mvwinsstr,' +
            'mvvline,napms,new_chstr,newpad,newwin,nl,nodelay,notimeout,noutrefresh,overlay,overwrite,pair_content,pechochar,' +
            'pnoutrefresh,prefresh,raw,redrawln,redrawwin,refresh,ripoffline,scrl,scrollok,slk_attroff,slk_attron,slk_attrset,' +
            'slk_clear,slk_init,slk_label,slk_noutrefresh,slk_refresh,slk_restore,slk_set,slk_touch,standend,standout,start_color,' +
            'stdscr,sub,subpad,syncdown,syncok,syncup,termattrs,termname,tigetflag,tigetnum,tigetstr,timeout,touch,touchline,' +
            'unctrl,ungetch,vline,wbkgd,wbkgdset,winch,winchnstr,winnstr,winsch,winsdelln,winsnstr,winsstr,wsetscrreg'
        end
        item
          FontID = 7
          Name = 'ival'
          Keywords =
            'ival,aeq,checkival,contained,diam,disjoint,eeq,eq,extremes,inf,isival,join,le,lt,mean,meet,member,new,' +
            'sup,tostring,xabs,xadd,xarccos,xarccosh,xarccot,xarccoth,xarcsin,xarcsinh,xarctan,xarctanh,xcbrt,xcos,' +
            'xcosh,xcot,xcoth,xdiv,xerf,xerfc,xexp,xexp10,xexp2,xexpm1,xhypot,xln,xlnp1,xlog10,xlog2,xmul,xneg,' +
            'xpow,xpytha,xrecip,xsin,xsinh,xsqrt,xsquare,xsub,xtan,xtanh'
        end
        item
          FontID = 7
          Name = 'strhash'
          Keywords =
            'strhash,attrib,getitem,has,include,iterate,new,purge,resize,totable'
        end
        item
          FontID = 7
          Name = 'strmap'
          Keywords =
            'strmap,attrib,cleanse,getitem,has,include,iterate,new,purge,resize,toseq,totable'
        end
        item
          FontID = 7
          Name = 'uintmap'
          Keywords =
            'uintmap,attrib,cleanse,getitem,has,hash,include,iterate,new,purge,resize,toseq,totable'
        end
        item
          FontID = 7
          Name = 'intmap'
          Keywords =
            'intmap,attrib,cleanse,getitem,has,include,iterate,new,purge,remap,resize,totable'
        end
        item
          FontID = 7
          Name = 'dblmap'
          Keywords =
            'dblmap,attrib,cleanse,getitem,has,include,iterate,new,purge,remap,resize,totable'
        end
        item
          FontID = 7
          Name = 'hashes'
          Keywords =
            'hashes,adler32,ap,asu,bkdr,bp,bsd,ccitt,cksum,collisions,crc16,crc32,crc7,crc8,damm,dek,derpy,digitsum,djb,djb2,' +
            'djb2rot,droot,elf,fibmod,fibmod2,finger,fletcher,fnv,ftok,hashfloat,hashmap,internet,interweave,ispell,j32to32,' +
            'jen,jinteger,jnumber,lua,luhn,md5,mix,mix64,mix64to32,murmur1,murmur2,murmur3,murmur3128,murmur3128x86,oaat,' +
            'parity,pjw,pl,raw,reflect,roaat,rs,sax,sdbm,sha256,sha512,squirrel32,squirrel64,sth,strval,sumupchars,superfast,' +
            'sysv,varlen,verhoeff,wang'
        end>
      OtherIdentChars = '%0-9A-Z_a-z'
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
      Font.Color = clRed
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 2
      GlobalAttrID = 'Float'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clRed
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 3
      GlobalAttrID = 'Comment'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clGray
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 4
      GlobalAttrID = 'String'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clMaroon
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 5
      GlobalAttrID = 'Reserved words'
      Font.Charset = DEFAULT_CHARSET
      Font.Color =  clNavy
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 6
      GlobalAttrID = 'Logical functions'
      Font.Charset = DEFAULT_CHARSET
      Font.Color =  clBlue
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 7
      GlobalAttrID = 'utilsfunctions'
      BlockID = 1
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clTeal
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 8
      GlobalAttrID = 'Delimiters'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clBlack
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
    end
    item
      FontID = 9
      GlobalAttrID = 'functions'
      Font.Charset = DEFAULT_CHARSET
      Font.Color =  clBlack
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 10
      GlobalAttrID = 'phonetiQsfunctions'
      BlockID = 1
      Font.Charset = DEFAULT_CHARSET
      Font.Color = ClMaroon
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end
    item
      FontID = 11
      GlobalAttrID = 'phonetiQstables'
      BlockID = 1
      Font.Charset = DEFAULT_CHARSET
      Font.Color = ClGreen
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = [fsBold]
    end>
  P.SyntaxVersion = 3
end