/* The following functions have been taken from
   https://people.sc.fsu.edu/~jburkardt/c_src/correlation/correlation.html and other pages on the site.

   Except where noted otherwise, the functions have been written by John Bukhardt.

   All the functions are MIT-licenced.

   Changes by a. walz:
   - The embedded lookup tables have been externalised to speed up read access.
   - r8_sign has been replaced with faster tools_signum.
   - Calls to r8_mach have been replaced with table look-ups.
   - Calls to slow i4_min, i4_max have been replaced.
   - i4_modp has been extended with a return code to avoid harsh exits.
   - Where applicable, if rules haven been violated, functions will now return NULL
     or AGN_NAN instead of just exiting.
*/

#define jbmath_c
#define LUA_LIB

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "agnhlps.h"
#include "jbmath.h"


static double *r8vec_normal_01_new (int n, int *seed);
static double *r8vec_linspace_new (int n, double a, double b);
static double *r8mat_normal_01_new (int m, int n, int *seed);
static double *r8mat_mm_new (int n1, int n2, int n3, double a[], double b[]);
static double *r8mat_copy_new (int m, int n, double a1[]);
static double *r8_besks (double xnu, double x, int nin);
static void   tred2 (int n, double a[], double d[], double e[], double z[]);
static int    tql2 (int n, double d[], double e[], double z[]);
static void   r8_knus (double xnu, double x, double *bknu, double *bknu1, int *iswtch);
static double *r8vec_uniform_01_new (int n, int *seed);


static double r8_machlu[6] = {  /* machine constants */
  0.0,                    4.450147717014403E-308,
  8.988465674311579E+307, 1.110223024625157E-016,
  2.220446049250313E-016, 0.301029995663981E+000 };


/******************************************************************************/
/*
  Purpose:
    R8_CSEVL evaluates a Chebyshev series.
  Modified:
    17 January 2012
  Reference:
    Roger Broucke,
    Algorithm 446:
    Ten Subroutines for the Manipulation of Chebyshev Series,
    Communications of the ACM,
    Volume 16, Number 4, April 1973, pages 254-256.
  Parameters:
    Input, double X, the evaluation point.
    Input, double CS[N], the Chebyshev coefficients.
    Input, int N, the number of Chebyshev coefficients.
    Output, double R8_CSEVL, the Chebyshev series evaluated at X.
*/
double r8_csevl (double x, double a[], int n) {
  int i;
  double b0 = 0.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double twox = 0.0;
  double value = 0.0;
  if (n < 1 || 1000 < n || x < -1.1 || 1.1 < x){
    return AGN_NAN;
  }
  twox = 2.0*x;
  b1 = 0.0;
  b0 = 0.0;
  for (i=n - 1; 0 <= i; i--) {
    b2 = b1;
    b1 = b0;
    /* b0 = twox*b1 - b2 + a[i]; */
    b0 = fma(twox, b1, -b2 + a[i]);  /* 6.4.10 improvement */
  }
  value = 0.5*(b0 - b2);
  return value;
}


/******************************************************************************/
/*
  Purpose:
    R8_B0MP evaluates the modulus and phase for the Bessel J0 and Y0 functions.
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double *AMPL, *THETA, the modulus and phase.
*/

static double bm0cs[37] = {
  +0.9211656246827742712573767730182E-01,
  -0.1050590997271905102480716371755E-02,
  +0.1470159840768759754056392850952E-04,
  -0.5058557606038554223347929327702E-06,
  +0.2787254538632444176630356137881E-07,
  -0.2062363611780914802618841018973E-08,
  +0.1870214313138879675138172596261E-09,
  -0.1969330971135636200241730777825E-10,
  +0.2325973793999275444012508818052E-11,
  -0.3009520344938250272851224734482E-12,
  +0.4194521333850669181471206768646E-13,
  -0.6219449312188445825973267429564E-14,
  +0.9718260411336068469601765885269E-15,
  -0.1588478585701075207366635966937E-15,
  +0.2700072193671308890086217324458E-16,
  -0.4750092365234008992477504786773E-17,
  +0.8615128162604370873191703746560E-18,
  -0.1605608686956144815745602703359E-18,
  +0.3066513987314482975188539801599E-19,
  -0.5987764223193956430696505617066E-20,
  +0.1192971253748248306489069841066E-20,
  -0.2420969142044805489484682581333E-21,
  +0.4996751760510616453371002879999E-22,
  -0.1047493639351158510095040511999E-22,
  +0.2227786843797468101048183466666E-23,
  -0.4801813239398162862370542933333E-24,
  +0.1047962723470959956476996266666E-24,
  -0.2313858165678615325101260800000E-25,
  +0.5164823088462674211635199999999E-26,
  -0.1164691191850065389525401599999E-26,
  +0.2651788486043319282958336000000E-27,
  -0.6092559503825728497691306666666E-28,
  +0.1411804686144259308038826666666E-28,
  -0.3298094961231737245750613333333E-29,
  +0.7763931143074065031714133333333E-30,
  -0.1841031343661458478421333333333E-30,
  +0.4395880138594310737100799999999E-31 };

static double bm02cs[40] = {
  +0.9500415145228381369330861335560E-01,
  -0.3801864682365670991748081566851E-03,
  +0.2258339301031481192951829927224E-05,
  -0.3895725802372228764730621412605E-07,
  +0.1246886416512081697930990529725E-08,
  -0.6065949022102503779803835058387E-10,
  +0.4008461651421746991015275971045E-11,
  -0.3350998183398094218467298794574E-12,
  +0.3377119716517417367063264341996E-13,
  -0.3964585901635012700569356295823E-14,
  +0.5286111503883857217387939744735E-15,
  -0.7852519083450852313654640243493E-16,
  +0.1280300573386682201011634073449E-16,
  -0.2263996296391429776287099244884E-17,
  +0.4300496929656790388646410290477E-18,
  -0.8705749805132587079747535451455E-19,
  +0.1865862713962095141181442772050E-19,
  -0.4210482486093065457345086972301E-20,
  +0.9956676964228400991581627417842E-21,
  -0.2457357442805313359605921478547E-21,
  +0.6307692160762031568087353707059E-22,
  -0.1678773691440740142693331172388E-22,
  +0.4620259064673904433770878136087E-23,
  -0.1311782266860308732237693402496E-23,
  +0.3834087564116302827747922440276E-24,
  -0.1151459324077741271072613293576E-24,
  +0.3547210007523338523076971345213E-25,
  -0.1119218385815004646264355942176E-25,
  +0.3611879427629837831698404994257E-26,
  -0.1190687765913333150092641762463E-26,
  +0.4005094059403968131802476449536E-27,
  -0.1373169422452212390595193916017E-27,
  +0.4794199088742531585996491526437E-28,
  -0.1702965627624109584006994476452E-28,
  +0.6149512428936330071503575161324E-29,
  -0.2255766896581828349944300237242E-29,
  +0.8399707509294299486061658353200E-30,
  -0.3172997595562602355567423936152E-30,
  +0.1215205298881298554583333026514E-30,
  -0.4715852749754438693013210568045E-31 };

static double bt02cs[39] = {
  -0.24548295213424597462050467249324,
  +0.12544121039084615780785331778299E-02,
  -0.31253950414871522854973446709571E-04,
  +0.14709778249940831164453426969314E-05,
  -0.99543488937950033643468850351158E-07,
  +0.85493166733203041247578711397751E-08,
  -0.86989759526554334557985512179192E-09,
  +0.10052099533559791084540101082153E-09,
  -0.12828230601708892903483623685544E-10,
  +0.17731700781805131705655750451023E-11,
  -0.26174574569485577488636284180925E-12,
  +0.40828351389972059621966481221103E-13,
  -0.66751668239742720054606749554261E-14,
  +0.11365761393071629448392469549951E-14,
  -0.20051189620647160250559266412117E-15,
  +0.36497978794766269635720591464106E-16,
  -0.68309637564582303169355843788800E-17,
  +0.13107583145670756620057104267946E-17,
  -0.25723363101850607778757130649599E-18,
  +0.51521657441863959925267780949333E-19,
  -0.10513017563758802637940741461333E-19,
  +0.21820381991194813847301084501333E-20,
  -0.46004701210362160577225905493333E-21,
  +0.98407006925466818520953651199999E-22,
  -0.21334038035728375844735986346666E-22,
  +0.46831036423973365296066286933333E-23,
  -0.10400213691985747236513382399999E-23,
  +0.23349105677301510051777740800000E-24,
  -0.52956825323318615788049749333333E-25,
  +0.12126341952959756829196287999999E-25,
  -0.28018897082289428760275626666666E-26,
  +0.65292678987012873342593706666666E-27,
  -0.15337980061873346427835733333333E-27,
  +0.36305884306364536682359466666666E-28,
  -0.86560755713629122479172266666666E-29,
  +0.20779909972536284571238399999999E-29,
  -0.50211170221417221674325333333333E-30,
  +0.12208360279441714184191999999999E-30,
  -0.29860056267039913454250666666666E-31 };

static double bth0cs[44] = {
  -0.24901780862128936717709793789967,
  +0.48550299609623749241048615535485E-03,
  -0.54511837345017204950656273563505E-05,
  +0.13558673059405964054377445929903E-06,
  -0.55691398902227626227583218414920E-08,
  +0.32609031824994335304004205719468E-09,
  -0.24918807862461341125237903877993E-10,
  +0.23449377420882520554352413564891E-11,
  -0.26096534444310387762177574766136E-12,
  +0.33353140420097395105869955014923E-13,
  -0.47890000440572684646750770557409E-14,
  +0.75956178436192215972642568545248E-15,
  -0.13131556016891440382773397487633E-15,
  +0.24483618345240857495426820738355E-16,
  -0.48805729810618777683256761918331E-17,
  +0.10327285029786316149223756361204E-17,
  -0.23057633815057217157004744527025E-18,
  +0.54044443001892693993017108483765E-19,
  -0.13240695194366572724155032882385E-19,
  +0.33780795621371970203424792124722E-20,
  -0.89457629157111779003026926292299E-21,
  +0.24519906889219317090899908651405E-21,
  -0.69388422876866318680139933157657E-22,
  +0.20228278714890138392946303337791E-22,
  -0.60628500002335483105794195371764E-23,
  +0.18649748964037635381823788396270E-23,
  -0.58783732384849894560245036530867E-24,
  +0.18958591447999563485531179503513E-24,
  -0.62481979372258858959291620728565E-25,
  +0.21017901684551024686638633529074E-25,
  -0.72084300935209253690813933992446E-26,
  +0.25181363892474240867156405976746E-26,
  -0.89518042258785778806143945953643E-27,
  +0.32357237479762298533256235868587E-27,
  -0.11883010519855353657047144113796E-27,
  +0.44306286907358104820579231941731E-28,
  -0.16761009648834829495792010135681E-28,
  +0.64292946921207466972532393966088E-29,
  -0.24992261166978652421207213682763E-29,
  +0.98399794299521955672828260355318E-30,
  -0.39220375242408016397989131626158E-30,
  +0.15818107030056522138590618845692E-30,
  -0.64525506144890715944344098365426E-31,
  +0.26611111369199356137177018346367E-31 };

int r8_b0mp (double x, double *ampl, double *theta) {
  double eta;
  static int nbm0 = 0;
  static int nbm02 = 0;
  static int nbt02 = 0;
  static int nbth0 = 0;
  static double pi4 = 0.785398163397448309615660845819876;
  double z;
  if (nbm0 == 0) {
    eta = 0.1*r8_machlu[3];
    nbm0 = r8_inits(bm0cs,  37, eta);
    nbt02 = r8_inits(bt02cs, 39, eta);
    nbm02 = r8_inits(bm02cs, 40, eta);
    nbth0 = r8_inits(bth0cs, 44, eta);
  }
  if (x < 4.0) {
    return 1;
  } else if (x <= 8.0) {
    z = (128.0/x/x - 5.0)/3.0;
    *ampl = (0.75 + r8_csevl(z, bm0cs, nbm0))/sqrt(x);
    *theta = x - pi4 + r8_csevl(z, bt02cs, nbt02 )/x;
  } else {
    z = 128.0/x/x - 1.0;
    *ampl = (0.75 + r8_csevl(z, bm02cs, nbm02))/sqrt(x);
    *theta = x - pi4 + r8_csevl(z, bth0cs, nbth0)/x;
  }
  return 0;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESI1 evaluates the Bessel function I of order 1 of an R8 argument.
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double R8_BESI1, the Bessel function I of order 1 of X.
*/

static double bi1cs[17] = {
  -0.19717132610998597316138503218149E-02,
  +0.40734887667546480608155393652014,
  +0.34838994299959455866245037783787E-01,
  +0.15453945563001236038598401058489E-02,
  +0.41888521098377784129458832004120E-04,
  +0.76490267648362114741959703966069E-06,
  +0.10042493924741178689179808037238E-07,
  +0.99322077919238106481371298054863E-10,
  +0.76638017918447637275200171681349E-12,
  +0.47414189238167394980388091948160E-14,
  +0.24041144040745181799863172032000E-16,
  +0.10171505007093713649121100799999E-18,
  +0.36450935657866949458491733333333E-21,
  +0.11205749502562039344810666666666E-23,
  +0.29875441934468088832000000000000E-26,
  +0.69732310939194709333333333333333E-29,
  +0.14367948220620800000000000000000E-31 };

double r8_besi1 (double x) {
  static int nti1 = 0;
  double value;
  static double xmax = 0.0;
  static double xmin = 0.0;
  static double xsml = 0.0;
  double y;
  if (nti1 == 0) {
    nti1 = r8_inits(bi1cs, 17, 0.1*r8_machlu[3]);
    xmin = 2.0*r8_machlu[1];
    xsml = sqrt(8.0*r8_machlu[3]);
    xmax = sun_log(r8_machlu[2]);
  }
  y = fabs(x);
  if (y <= xmin) {
    value = 0.0;
  } else if (y <= xsml) {
    value = 0.5*x;
  } else if (y <= 3.0) {
    value = x*(0.875 + r8_csevl(y*y/4.5 - 1.0, bi1cs, nti1));
  } else if (y <= xmax) {
    value = sun_exp(y)*r8_besi1e(x);
  } else {
    return AGN_NAN;
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESI1E evaluates the exponentially scaled Bessel function I1(X).
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double R8_BESI1E, the exponentially scaled Bessel
    function I1(X).
*/

static double ai12cs[69] = {
  +0.2857623501828012047449845948469E-01,
  -0.9761097491361468407765164457302E-02,
  -0.1105889387626237162912569212775E-03,
  -0.3882564808877690393456544776274E-05,
  -0.2512236237870208925294520022121E-06,
  -0.2631468846889519506837052365232E-07,
  -0.3835380385964237022045006787968E-08,
  -0.5589743462196583806868112522229E-09,
  -0.1897495812350541234498925033238E-10,
  +0.3252603583015488238555080679949E-10,
  +0.1412580743661378133163366332846E-10,
  +0.2035628544147089507224526136840E-11,
  -0.7198551776245908512092589890446E-12,
  -0.4083551111092197318228499639691E-12,
  -0.2101541842772664313019845727462E-13,
  +0.4272440016711951354297788336997E-13,
  +0.1042027698412880276417414499948E-13,
  -0.3814403072437007804767072535396E-14,
  -0.1880354775510782448512734533963E-14,
  +0.3308202310920928282731903352405E-15,
  +0.2962628997645950139068546542052E-15,
  -0.3209525921993423958778373532887E-16,
  -0.4650305368489358325571282818979E-16,
  +0.4414348323071707949946113759641E-17,
  +0.7517296310842104805425458080295E-17,
  -0.9314178867326883375684847845157E-18,
  -0.1242193275194890956116784488697E-17,
  +0.2414276719454848469005153902176E-18,
  +0.2026944384053285178971922860692E-18,
  -0.6394267188269097787043919886811E-19,
  -0.3049812452373095896084884503571E-19,
  +0.1612841851651480225134622307691E-19,
  +0.3560913964309925054510270904620E-20,
  -0.3752017947936439079666828003246E-20,
  -0.5787037427074799345951982310741E-22,
  +0.7759997511648161961982369632092E-21,
  -0.1452790897202233394064459874085E-21,
  -0.1318225286739036702121922753374E-21,
  +0.6116654862903070701879991331717E-22,
  +0.1376279762427126427730243383634E-22,
  -0.1690837689959347884919839382306E-22,
  +0.1430596088595433153987201085385E-23,
  +0.3409557828090594020405367729902E-23,
  -0.1309457666270760227845738726424E-23,
  -0.3940706411240257436093521417557E-24,
  +0.4277137426980876580806166797352E-24,
  -0.4424634830982606881900283123029E-25,
  -0.8734113196230714972115309788747E-25,
  +0.4045401335683533392143404142428E-25,
  +0.7067100658094689465651607717806E-26,
  -0.1249463344565105223002864518605E-25,
  +0.2867392244403437032979483391426E-26,
  +0.2044292892504292670281779574210E-26,
  -0.1518636633820462568371346802911E-26,
  +0.8110181098187575886132279107037E-28,
  +0.3580379354773586091127173703270E-27,
  -0.1692929018927902509593057175448E-27,
  -0.2222902499702427639067758527774E-28,
  +0.5424535127145969655048600401128E-28,
  -0.1787068401578018688764912993304E-28,
  -0.6565479068722814938823929437880E-29,
  +0.7807013165061145280922067706839E-29,
  -0.1816595260668979717379333152221E-29,
  -0.1287704952660084820376875598959E-29,
  +0.1114548172988164547413709273694E-29,
  -0.1808343145039336939159368876687E-30,
  -0.2231677718203771952232448228939E-30,
  +0.1619029596080341510617909803614E-30,
  -0.1834079908804941413901308439210E-31 };

static double ai1cs[46] = {
  -0.2846744181881478674100372468307E-01,
  -0.1922953231443220651044448774979E-01,
  -0.6115185857943788982256249917785E-03,
  -0.2069971253350227708882823777979E-04,
  +0.8585619145810725565536944673138E-05,
  +0.1049498246711590862517453997860E-05,
  -0.2918338918447902202093432326697E-06,
  -0.1559378146631739000160680969077E-07,
  +0.1318012367144944705525302873909E-07,
  -0.1448423418183078317639134467815E-08,
  -0.2908512243993142094825040993010E-09,
  +0.1266388917875382387311159690403E-09,
  -0.1664947772919220670624178398580E-10,
  -0.1666653644609432976095937154999E-11,
  +0.1242602414290768265232168472017E-11,
  -0.2731549379672432397251461428633E-12,
  +0.2023947881645803780700262688981E-13,
  +0.7307950018116883636198698126123E-14,
  -0.3332905634404674943813778617133E-14,
  +0.7175346558512953743542254665670E-15,
  -0.6982530324796256355850629223656E-16,
  -0.1299944201562760760060446080587E-16,
  +0.8120942864242798892054678342860E-17,
  -0.2194016207410736898156266643783E-17,
  +0.3630516170029654848279860932334E-18,
  -0.1695139772439104166306866790399E-19,
  -0.1288184829897907807116882538222E-19,
  +0.5694428604967052780109991073109E-20,
  -0.1459597009090480056545509900287E-20,
  +0.2514546010675717314084691334485E-21,
  -0.1844758883139124818160400029013E-22,
  -0.6339760596227948641928609791999E-23,
  +0.3461441102031011111108146626560E-23,
  -0.1017062335371393547596541023573E-23,
  +0.2149877147090431445962500778666E-24,
  -0.3045252425238676401746206173866E-25,
  +0.5238082144721285982177634986666E-27,
  +0.1443583107089382446416789503999E-26,
  -0.6121302074890042733200670719999E-27,
  +0.1700011117467818418349189802666E-27,
  -0.3596589107984244158535215786666E-28,
  +0.5448178578948418576650513066666E-29,
  -0.2731831789689084989162564266666E-30,
  -0.1858905021708600715771903999999E-30,
  +0.9212682974513933441127765333333E-31,
  -0.2813835155653561106370833066666E-31 };

double r8_besi1e (double x) {
  double y, eta, value;
  static int ntai1 = 0;
  static int ntai12 = 0;
  static int nti1 = 0;
  static double xmin = 0.0;
  static double xsml = 0.0;
  if (nti1 == 0) {
    eta = 0.1*r8_machlu[3];
    nti1 = r8_inits(bi1cs, 17, eta);
    ntai1 = r8_inits(ai1cs, 46, eta);
    ntai12 = r8_inits(ai12cs, 69, eta);
    xmin = 2.0*r8_machlu[1];
    xsml = sqrt(8.0*r8_machlu[3]);
  }
  y = fabs(x);
  if (y <= xmin) {
    value = 0.0;
  } else if (y <= xsml) {
    value = 0.5*x*sun_exp(-y);
  } else if (y <= 3.0) {
    value = x*(0.875 + r8_csevl(y*y/4.5 - 1.0, bi1cs, nti1))*sun_exp(-y);
  } else if (y <= 8.0) {
    value = (0.375 + r8_csevl((48.0/y - 11.0)/5.0, ai1cs, ntai1))/sqrt(y);
    if (x < 0.0) value = - value;
  } else {
    value = (0.375 + r8_csevl(16.0/y - 1.0, ai12cs, ntai12))/sqrt(y);
    if (x < 0.0) value = -value;
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESJ0 evaluates the Bessel function J of order 0 of an R8 argument.
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double R8_BESJ0, the Bessel function J of order 0 of X.
*/

static double bj0cs[19] = {
  +0.10025416196893913701073127264074,
  -0.66522300776440513177678757831124,
  +0.24898370349828131370460468726680,
  -0.33252723170035769653884341503854E-01,
  +0.23114179304694015462904924117729E-02,
  -0.99112774199508092339048519336549E-04,
  +0.28916708643998808884733903747078E-05,
  -0.61210858663032635057818407481516E-07,
  +0.98386507938567841324768748636415E-09,
  -0.12423551597301765145515897006836E-10,
  +0.12654336302559045797915827210363E-12,
  -0.10619456495287244546914817512959E-14,
  +0.74706210758024567437098915584000E-17,
  -0.44697032274412780547627007999999E-19,
  +0.23024281584337436200523093333333E-21,
  -0.10319144794166698148522666666666E-23,
  +0.40608178274873322700800000000000E-26,
  -0.14143836005240913919999999999999E-28,
  +0.43910905496698880000000000000000E-31 };

double r8_besj0 (double x) {
  double ampl, theta, value, y;
  static int ntj0 = 0;
  static double xsml = 0.0;
  if (ntj0 == 0) {
    ntj0 = r8_inits(bj0cs, 19, 0.1*r8_machlu[3]);
    xsml = sqrt(4.0*r8_machlu[3]);
  }
  y = fabs(x);
  if (y <= xsml) {
    value = 1.0;
  } else if (y <= 4.0) {
    value = r8_csevl(0.125*y*y - 1.0, bj0cs, ntj0);
  } else {
    int rc = r8_b0mp(y, &ampl, &theta);
    if (rc) return AGN_NAN;
    value = ampl*sun_cos(theta);
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESK evaluates the Bessel function K of order NU of an R8 argument.
  Modified:
    03 November 2012
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double NU, the order.
    Input, double X, the argument.
    Output, double R8_BESK, the Bessel function K of order NU at X.
*/
double r8_besk (double nu, double x) {
  double *bke, value, xnu;
  int nin;
  xnu = nu - (int)(nu);
  nin = (int)(nu) + 1;
  bke = r8_besks(xnu, x, nin);
  value = bke[nin - 1];
  free(bke);
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESK1 evaluates the Bessel function K of order 1 of an R8 argument.
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double R8_BESK1, the Bessel function K of order 1 of X.
*/

static double bk1cs[16] = {
  +0.25300227338947770532531120868533E-01,
  -0.35315596077654487566723831691801,
  -0.12261118082265714823479067930042,
  -0.69757238596398643501812920296083E-02,
  -0.17302889575130520630176507368979E-03,
  -0.24334061415659682349600735030164E-05,
  -0.22133876307347258558315252545126E-07,
  -0.14114883926335277610958330212608E-09,
  -0.66669016941993290060853751264373E-12,
  -0.24274498505193659339263196864853E-14,
  -0.70238634793862875971783797120000E-17,
  -0.16543275155100994675491029333333E-19,
  -0.32338347459944491991893333333333E-22,
  -0.53312750529265274999466666666666E-25,
  -0.75130407162157226666666666666666E-28,
  -0.91550857176541866666666666666666E-31 };

double r8_besk1 (double x) {
  static int ntk1 = 0;
  double value, y;
  static double xmax = 0.0;
  static double xsml = 0.0;
  if (ntk1 == 0) {
    ntk1 = r8_inits(bk1cs, 16, 0.1*r8_machlu[3]);
    xsml = sqrt(4.0*r8_machlu[3]);
    xmax = - sun_log(r8_machlu[1]);
    xmax = xmax - 0.5*xmax*sun_log(xmax)/(xmax + 0.5) - 0.01;
  }
  if (x <= 0.0) {
    return AGN_NAN;
  } else if (x <= xsml) {
    y = 0.0;
    value = sun_log(0.5*x)*r8_besi1(x) + (0.75 + r8_csevl(0.5*y - 1.0, bk1cs, ntk1))/x;
  } else if (x <= 2.0) {
    y = x*x;
    value = sun_log(0.5*x)*r8_besi1(x) + (0.75 + r8_csevl(0.5*y - 1.0, bk1cs, ntk1))/x;
  } else if (x <= xmax) {
    value = sun_exp(- x)*r8_besk1e(x);
  } else {
    value = 0.0;
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESK1E evaluates the exponentially scaled Bessel function K1(X).
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double R8_BESK1E, the exponentially scaled Bessel
    function K1(X).
*/

static double ak12cs[33] = {
  +0.6379308343739001036600488534102E-01,
  +0.2832887813049720935835030284708E-01,
  -0.2475370673905250345414545566732E-03,
  +0.5771972451607248820470976625763E-05,
  -0.2068939219536548302745533196552E-06,
  +0.9739983441381804180309213097887E-08,
  -0.5585336140380624984688895511129E-09,
  +0.3732996634046185240221212854731E-10,
  -0.2825051961023225445135065754928E-11,
  +0.2372019002484144173643496955486E-12,
  -0.2176677387991753979268301667938E-13,
  +0.2157914161616032453939562689706E-14,
  -0.2290196930718269275991551338154E-15,
  +0.2582885729823274961919939565226E-16,
  -0.3076752641268463187621098173440E-17,
  +0.3851487721280491597094896844799E-18,
  -0.5044794897641528977117282508800E-19,
  +0.6888673850418544237018292223999E-20,
  -0.9775041541950118303002132480000E-21,
  +0.1437416218523836461001659733333E-21,
  -0.2185059497344347373499733333333E-22,
  +0.3426245621809220631645388800000E-23,
  -0.5531064394246408232501248000000E-24,
  +0.9176601505685995403782826666666E-25,
  -0.1562287203618024911448746666666E-25,
  +0.2725419375484333132349439999999E-26,
  -0.4865674910074827992378026666666E-27,
  +0.8879388552723502587357866666666E-28,
  -0.1654585918039257548936533333333E-28,
  +0.3145111321357848674303999999999E-29,
  -0.6092998312193127612416000000000E-30,
  +0.1202021939369815834623999999999E-30,
  -0.2412930801459408841386666666666E-31 };

static double ak1cs[38] = {
  +0.27443134069738829695257666227266,
  +0.75719899531993678170892378149290E-01,
  -0.14410515564754061229853116175625E-02,
  +0.66501169551257479394251385477036E-04,
  -0.43699847095201407660580845089167E-05,
  +0.35402774997630526799417139008534E-06,
  -0.33111637792932920208982688245704E-07,
  +0.34459775819010534532311499770992E-08,
  -0.38989323474754271048981937492758E-09,
  +0.47208197504658356400947449339005E-10,
  -0.60478356628753562345373591562890E-11,
  +0.81284948748658747888193837985663E-12,
  -0.11386945747147891428923915951042E-12,
  +0.16540358408462282325972948205090E-13,
  -0.24809025677068848221516010440533E-14,
  +0.38292378907024096948429227299157E-15,
  -0.60647341040012418187768210377386E-16,
  +0.98324256232648616038194004650666E-17,
  -0.16284168738284380035666620115626E-17,
  +0.27501536496752623718284120337066E-18,
  -0.47289666463953250924281069568000E-19,
  +0.82681500028109932722392050346666E-20,
  -0.14681405136624956337193964885333E-20,
  +0.26447639269208245978085894826666E-21,
  -0.48290157564856387897969868800000E-22,
  +0.89293020743610130180656332799999E-23,
  -0.16708397168972517176997751466666E-23,
  +0.31616456034040694931368618666666E-24,
  -0.60462055312274989106506410666666E-25,
  +0.11678798942042732700718421333333E-25,
  -0.22773741582653996232867840000000E-26,
  +0.44811097300773675795305813333333E-27,
  -0.88932884769020194062336000000000E-28,
  +0.17794680018850275131392000000000E-28,
  -0.35884555967329095821994666666666E-29,
  +0.72906290492694257991679999999999E-30,
  -0.14918449845546227073024000000000E-30,
  +0.30736573872934276300799999999999E-31 };

double r8_besk1e (double x) {
  double value, y, eta;
  static int ntak1 = 0;
  static int ntak12 = 0;
  static int ntk1 = 0;
  static double xsml = 0.0;
  if (ntk1 == 0) {
    eta = 0.1*r8_machlu[3];
    ntk1 = r8_inits(bk1cs, 16, eta);
    ntak1 = r8_inits(ak1cs, 38, eta);
    ntak12 = r8_inits(ak12cs, 33, eta);
    xsml = sqrt(4.0*r8_machlu[3]);
  }
  if (x <= 0.0) {
    return AGN_NAN;
  } else if (x <= xsml) {
    y = 0.0;
    value = sun_exp(x)*(sun_log(0.5*x)*r8_besi1(x) + (0.75 + r8_csevl(0.5*y - 1.0, bk1cs, ntk1))/x);
  } else if (x <= 2.0) {
    y = x*x;
    value = sun_exp(x)*(sun_log(0.5*x)*r8_besi1(x) + (0.75 + r8_csevl(0.5*y - 1.0, bk1cs, ntk1))/x);
  } else if (x <= 8.0) {
    value = (1.25 + r8_csevl((16.0/x - 5.0)/3.0, ak1cs, ntak1))/sqrt(x);
  } else {
    value = (1.25 + r8_csevl(16.0/x - 1.0, ak12cs, ntak12))/sqrt(x);
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESKES: a sequence of exponentially scaled K Bessel functions at X.
  Modified:
    04 November 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double XNU, ?
    |XNU| < 1.
    Input, double X, the argument.
    Input, int NIN, indicates the number of terms to compute.
    Output, double R8_BESKES(abs(NIN)), the exponentially scaled
    K Bessel functions.
*/
static double *r8_beskes (double xnu, double x, int nin) {
  double *bke, bknu1, direct, v, vincr;
  int i, iswtch, n;
  v = fabs(xnu);
  n = abs(nin);
  if (1.0 <= v || x <= 0.0 || n == 0) return NULL;
  bke = (double *)malloc(abs(nin)*sizeof(double));
  r8_knus(v, x, &bke[0], &bknu1, &iswtch);
  if (n == 1) { return bke; }
  if (nin < 0) {
    vincr = - 1.0;
  } else {
    vincr = + 1.0;
  }
  if (xnu < 0.0) {
    direct = - vincr;
  } else {
    direct = vincr;
  }
  bke[1] = bknu1;
  if (direct < 0.0) {
    r8_knus(fabs(xnu + vincr), x, &bke[1], &bknu1, &iswtch);
  }
  if (n == 2) return bke;
  v = xnu;
  for (i=3; i <= n; i++) {
    v = v + vincr;
    bke[i-1] = 2.0*v*bke[i-2]/x + bke[i-3];
  }
  return bke;
}

/******************************************************************************/
/*
  Purpose:
    R8_BESKS evaluates a sequence of K Bessel functions at X.
  Modified:
    04 November 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double XNU, ?
    |XNU| < 1.
    Input, double X, the argument.
    Input, int NIN, indicates the number of terms to compute.
    Output, double R8_BESKS(abs(NIN)), the K Bessel functions.
*/
static double *r8_besks (double xnu, double x, int nin) {
  double *bk, expxi;
  int i, n;
  static double xmax = 0.0;
  if (xmax == 0.0) {
    xmax = -sun_log(r8_machlu[1]);
    xmax += 0.5*sun_log(3.14*0.5/xmax);
  }
  bk = r8_beskes(xnu, x, nin);
  expxi = sun_exp(-x);
  n = abs(nin);
  for (i=0; i < n; i++) {
    bk[i] = expxi*bk[i];
  }
  return bk;
}

/******************************************************************************/
/*
  Purpose:
    R8_INITS initializes a Chebyshev series.
  Modified:
    17 January 2012
  Reference:
    Roger Broucke,
    Algorithm 446:
    Ten Subroutines for the Manipulation of Chebyshev Series,
    Communications of the ACM,
    Volume 16, Number 4, April 1973, pages 254-256.
  Parameters:
    Input, double DOS[NOS], the Chebyshev coefficients.
    Input, int NOS, the number of coefficients.
    Input, double ETA, the desired accuracy.
    Output, int R8_INITS, the number of terms of the series needed
    to ensure the requested accuracy.
*/
int r8_inits (double dos[], int nos, double eta) {
  double err;
  int i, value;
  if (nos < 1) {
    fprintf(stderr, "\nR8_INITS Number of coefficients < 1.\n");
    return 0;
  }
  err = 0.0;
  for (i=nos - 1; 0 <= i; i--) {
    err = err + fabs(dos[i]);
    if (eta < err) {
      value = i + 1;
      return value;
    }
  }
  value = i;
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_KNUS computes a sequence of K Bessel functions.
  Discussion:
    This routine computes Bessel functions
      exp(x)*k-sub-xnu (x)
    and
      exp(x)*k-sub-xnu+1 (x)
    for 0.0 <= xnu < 1.0.
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double XNU, the order parameter.
    Input, double X, the argument.
    Output, double *BKNU, *BKNU1, the two K Bessel functions.
    Output, int *ISWTCH, ?
*/

static double c0kcs[29] = {
  +0.60183057242626108387577445180329E-01,
  -0.15364871433017286092959755943124,
  -0.11751176008210492040068229226213E-01,
  -0.85248788891979509827048401550987E-03,
  -0.61329838767496791874098176922111E-04,
  -0.44052281245510444562679889548505E-05,
  -0.31631246728384488192915445892199E-06,
  -0.22710719382899588330673771793396E-07,
  -0.16305644608077609552274620515360E-08,
  -0.11706939299414776568756044043130E-09,
  -0.84052063786464437174546593413792E-11,
  -0.60346670118979991487096050737198E-12,
  -0.43326960335681371952045997366903E-13,
  -0.31107358030203546214634697772237E-14,
  -0.22334078226736982254486133409840E-15,
  -0.16035146716864226300635791528610E-16,
  -0.11512717363666556196035697705305E-17,
  -0.82657591746836959105169479089258E-19,
  -0.59345480806383948172333436695984E-20,
  -0.42608138196467143926499613023976E-21,
  -0.30591266864812876299263698370542E-22,
  -0.21963541426734575224975501815516E-23,
  -0.15769113261495836071105750684760E-24,
  -0.11321713935950320948757731048056E-25,
  -0.81286248834598404082792349714433E-27,
  -0.58360900893453226552829349315949E-28,
  -0.41901241623610922519452337780905E-29,
  -0.30083737960206435069530504212862E-30,
  -0.21599152067808647728342168089832E-31 };

static double znu1cs[20] = {
  +0.203306756994191729674444001216911,
  +0.140077933413219771062943670790563,
  +0.791679696100161352840972241972320E-02,
  +0.339801182532104045352930092205750E-03,
  +0.117419756889893366664507228352690E-04,
  +0.339357570612261680333825865475121E-06,
  +0.842594176976219910194629891264803E-08,
  +0.183336677024850089184748150900090E-09,
  +0.354969844704416310863007064469557E-11,
  +0.619032496469887332205244342078407E-13,
  +0.981964535680439424960346115456527E-15,
  +0.142851314396490474211473563005985E-16,
  +0.191894921887825298966162467488436E-18,
  +0.239430979739498914162313140597128E-20,
  +0.278890246815347354835870465474995E-22,
  +0.304606650633033442582845214092865E-24,
  +0.313173237042191815771564260932089E-26,
  +0.304133098987854951645174908005034E-28,
  +0.279840384636833084343185097659733E-30,
  +0.244637186274497596485238794922666E-32 };

static void r8_knus (double xnu, double x, double *bknu, double *bknu1, int *iswtch) {
  double a[32], a0, alpha[32], alnz, an, b0, beta[32], bknu0, bknud, bn, c0, ztov,
         eta, expx, p1, p2, p3, qq, result, sqrtx, v, vlnz, x2n, x2tov, xi, xmu, z;
  static double aln2 = 0.69314718055994530941723212145818;
  static double alnbig = 0;
  static double alneps = 0;
  static double alnsml = 0;
  static double euler = 0.57721566490153286060651209008240;
  static double sqpi2 = +1.2533141373155002512078826424055;
  static double xnusml = 0.0;
  static double xsml = 0.0;
  int i, ii, inu, n, nterms;
  static int ntc0k = 0;
  static int ntznu1 = 0;
  if (ntc0k == 0) {
    eta = 0.1*r8_machlu[3];
    ntc0k = r8_inits(c0kcs, 29, eta);
    ntznu1 = r8_inits(znu1cs, 20, eta);
    xnusml = sqrt(r8_machlu[3]/8.0);
    xsml = 0.1*r8_machlu[3];
    alnsml = sun_log(r8_machlu[1]);
    alnbig = sun_log(r8_machlu[2]);
    alneps = sun_log(0.1*r8_machlu[3]);
  }
  if (xnu < 0.0 || 1.0 <= xnu) {
    fprintf(stderr, "\n");
    fprintf(stderr, "R8_KNUS - Fatal error!\n");
    fprintf(stderr, "  XNU < 0 or 1 <= XNU.\n");
    exit(1);
  }
  if (x <= 0.0) {
    fprintf(stderr, "\n");
    fprintf(stderr, "R8_KNUS - Fatal error!\n");
    fprintf(stderr, "  X <= 0.\n");
    exit(1);
  }
  *iswtch = 0;
/*
  X is small.  Compute k-sub-xnu (x) and the derivative of k-sub-xnu (x)
  then find k-sub-xnu+1 (x).  xnu is reduced to the interval (-0.5,+0.5)
  then to (0., .5), because k of negative order (-nu) = k of positive
  order (+nu).
*/
  if (x <= 2.0) {
    if (xnu <= 0.5) {
      v = xnu;
    } else {
      v = 1.0 - xnu;
    }
/*
  Carefully find (x/2)^xnu and z^xnu where z = x*x/4.
*/
    alnz = 2.0*(sun_log(x) - aln2);
    if (x <= xnu) {
      if (alnbig < - 0.5*xnu*alnz - aln2 - sun_log(xnu)) {
        fprintf(stderr, "\n");
        fprintf(stderr, "R8_KNUS - Fatal error!\n");
        fprintf(stderr, "  Small X causing overflow.\n");
        exit(1);
      }
    }
    vlnz = v*alnz;
    x2tov = sun_exp(0.5*vlnz);
    if (vlnz <= alnsml) {
      ztov = 0.0;
    } else {
      ztov = x2tov*x2tov;
    }
    a0 = 0.5*tgamma (1.0 + v);
    b0 = 0.5*tgamma (1.0 - v);
    c0 = -euler;
    if (0.5 <= ztov && xnusml < v) {
      c0 = - 0.75 + r8_csevl((8.0*v)* v - 1.0, c0kcs, ntc0k);
    }
    if (ztov <= 0.5) {
      alpha[0] = (a0 - ztov*b0)/v;
    } else {
      alpha[0] = c0 - alnz*(0.75 + r8_csevl(vlnz/0.35 + 1.0, znu1cs, ntznu1))* b0;
    }
    beta[0] = - 0.5*(a0 + ztov*b0);
    if (x <= xsml) {
      z = 0.0;
    } else {
      z = 0.25*x*x;
    }
    nterms = fMax(2, (int)(11.0 + (8.0*alnz - 25.19 - alneps)/(4.28 - alnz)));
    for (i=2; i <= nterms; i++) {
      xi = (double)(i - 1);
      a0 = a0/(xi*(xi - v));
      b0 = b0/(xi*(xi + v));
      alpha[i-1] = (alpha[i-2] + 2.0*xi*a0)/(xi*(xi + v));
      beta[i-1] = (xi - 0.5*v)*alpha[i-1] - ztov*b0;
    }
    *bknu = alpha[nterms-1];
    bknud = beta[nterms-1];
    for (ii = 2; ii <= nterms; ii++) {
      i = nterms + 1 - ii;
      *bknu = alpha[i-1] + *bknu*z;
      bknud = beta[i-1] + bknud*z;
    }
    expx = sun_exp(x);
    *bknu = expx**bknu/x2tov;
    if (alnbig < - 0.5*(xnu + 1.0)*alnz - 2.0*aln2) {
      *iswtch = 1;
      return;
    }
    bknud = expx*bknud*2.0/(x2tov*x);
    if (xnu <= 0.5) {
      *bknu1 = v**bknu/x - bknud;
      return;
    }
    bknu0 = *bknu;
    *bknu = - v**bknu/x - bknud;
    *bknu1 = 2.0*xnu**bknu/x + bknu0;
  }
/*
  X is large.  find k-sub-xnu (x) and k-sub-xnu+1 (x) with y. l. luke-s
  rational expansion.
*/
  else {
    sqrtx = sqrt(x);
    if (1.0/xsml < x) {
      *bknu = sqpi2/sqrtx;
      *bknu1 = *bknu;
      return;
    }
    an = -0.60 - 1.02/x;
    bn = -0.27 - 0.53/x;
    nterms = fMin(32, fMax(3, (int)(an + bn*alneps)));
    for (inu = 1; inu <= 2; inu++) {
      if (inu == 1) {
        if (xnu <= xnusml) {
          xmu = 0.0;
        } else {
          xmu = (4.0*xnu)*xnu;
        }
      } else {
        xmu = 4.0*(fabs(xnu) + 1.0)*(fabs(xnu) + 1.0);
      }
      a[0] = 1.0 - xmu;
      a[1] = 9.0 - xmu;
      a[2] = 25.0 - xmu;
      if (a[1] == 0.0) {
        result = sqpi2*(16.0*x + xmu + 7.0)/(16.0*x*sqrtx);
      } else {
        alpha[0] = 1.0;
        alpha[1] = (16.0*x + a[1])/a[1];
        alpha[2] = ((768.0*x + 48.0*a[2])*x + a[1]*a[2])/(a[1]*a[2]);
        beta[0] = 1.0;
        beta[1] = (16.0*x + (xmu + 7.0))/a[1];
        beta[2] = ((768.0*x + 48.0*(xmu + 23.0))* x + ((xmu + 62.0)*xmu + 129.0))/(a[1]*a[2]);
        for (i=4; i <= nterms; i++) {
          n = i - 1;
          x2n = (double)(2*n - 1);
          a[i-1] = (x2n + 2.0)*(x2n + 2.0) - xmu;
          qq = 16.0*x2n/a[i-1];
          p1 = -x2n*((double)(12*n*n - 20*n) - a[0])/((x2n - 2.0)* a[i-1]) - qq*x;
          p2 = ((double)(12*n*n - 28*n + 8) - a[0])/a[i-1] - qq*x;
          p3 = -x2n*a[i-4]/((x2n - 2.0)*a[i-1]);
          alpha[i-1] = -p1*alpha[i-2] - p2*alpha[i-3] - p3*alpha[i-4];
          beta[i-1] = -p1*beta[i-2] - p2*beta[i-3] - p3*beta[i-4];
        }
        result = sqpi2*beta[nterms-1]/(sqrtx*alpha[nterms-1]);
      }
      if (inu == 1) {
        *bknu = result;
      } else {
        *bknu1 = result;
      }
    }
  }
  return;
}

/******************************************************************************/
/*
  Purpose:
    R8_LGMC evaluates the log gamma correction factor for an R8 argument.
  Discussion:
    For 10 <= X, compute the log gamma correction factor so that
      log (gamma (x)) = log (sqrt(2*pi))
                          + (x - 0.5)* log (x) - x
                          + r8_lgmc(x)
  Modified:
    17 January 2012
  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.
  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.
  Parameters:
    Input, double X, the argument.
    Output, double R8_LGMC, the correction factor.
*/

static double algmcs[15] = {
  +0.1666389480451863247205729650822,
  -0.1384948176067563840732986059135E-04,
  +0.9810825646924729426157171547487E-08,
  -0.1809129475572494194263306266719E-10,
  +0.6221098041892605227126015543416E-13,
  -0.3399615005417721944303330599666E-15,
  +0.2683181998482698748957538846666E-17,
  -0.2868042435334643284144622399999E-19,
  +0.3962837061046434803679306666666E-21,
  -0.6831888753985766870111999999999E-23,
  +0.1429227355942498147573333333333E-24,
  -0.3547598158101070547199999999999E-26,
  +0.1025680058010470912000000000000E-27,
  -0.3401102254316748799999999999999E-29,
  +0.1276642195630062933333333333333E-30 };

double r8_lgmc (double x) {
  static int nalgm = 0;
  double value;
  static double xbig = 0.0;
  static double xmax = 0.0;
  if (nalgm == 0) {
    nalgm = r8_inits(algmcs, 15, r8_machlu[3]);
    xbig = 1.0/sqrt(r8_machlu[3]);
    xmax = sun_exp(fMin(sun_log(r8_machlu[2]/12.0), -sun_log(12.0*r8_machlu[1])));
  }
  if (x < 10.0) {
    return AGN_NAN;
  } else if (x < xbig) {
    value = r8_csevl(2.0*(10.0/x)
     *(10.0/x) - 1.0, algmcs, nalgm)/x;
  } else if (x < xmax) {
    value = 1.0/( 12.0*x);
  } else {
    value = 0.0;
  }
  return value;
}

/******************************************************************************/
/*
  DO NOT DELETE THIS CODE !

  Purpose:
    R8_MACH returns double precision real machine constants.
  Discussion:
    Assuming that the internal representation of a double precision real
    number is in base B, with T the number of base-B digits in the mantissa,
    and EMIN the smallest possible exponent and EMAX the largest possible
    exponent, then
      R8_MACH(1) = B^(EMIN-1), the smallest positive magnitude.
      R8_MACH(2) = B^EMAX*(1-B^(-T)), the largest magnitude.
      R8_MACH(3) = B^(-T), the smallest relative spacing.
      R8_MACH(4) = B^(1-T), the largest relative spacing.
      R8_MACH(5) = log10(B).
  Modified:
    24 April 2007
  Author:
    Original FORTRAN77 version by Phyllis Fox, Andrew Hall, Norman Schryer.
    C version by John Burkardt.
  Reference:
    Phyllis Fox, Andrew Hall, Norman Schryer,
    Algorithm 528:
    Framework for a Portable Library,
    ACM Transactions on Mathematical Software,
    Volume 4, Number 2, June 1978, page 176-188.
  Parameters:
    Input, int I, chooses the parameter to be returned.
    1 <= I <= 5.
    Output, double R8_MACH, the value of the chosen parameter.
*/
double r8_mach (int i) {
  double value = 0;
  if (i < 1 || i > 5) {
    fprintf(stderr, "\nR8_MACH - inout is not in [1, 5].\n");
    value = AGN_NAN;
  } else if (i == 1) {
    value = 4.450147717014403E-308;
  } else if (i == 2) {
    value = 8.988465674311579E+307;
  } else if (i == 3) {
    value = 1.110223024625157E-016;
  } else if (i == 4) {
    value = 2.220446049250313E-016;
  } else {  /* i == 5 */
    value = 0.301029995663981E+000;
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8_UNIFORM_01 returns a pseudorandom R8 scaled to [0,1].
  Discussion:
    This routine implements the recursion
      seed = 16807*seed mod (2^31 - 1 )
      r8_uniform_01 = seed/( 2^31 - 1 )
    The integer arithmetic never requires more than 32 bits,
    including a sign bit.
    If the initial seed is 12345, then the first three computations are
      Input     Output      R8_UNIFORM_01
      SEED      SEED
         12345   207482415  0.096616
     207482415  1790989824  0.833995
    1790989824  2035175616  0.947702
  Modified:
    11 August 2004
  Reference:
    Paul Bratley, Bennett Fox, Linus Schrage,
    A Guide to Simulation,
    Springer Verlag, pages 201-202, 1983.
    Pierre L'Ecuyer,
    Random Number Generation,
    in Handbook of Simulation
    edited by Jerry Banks,
    Wiley Interscience, page 95, 1998.
    Bennett Fox,
    Algorithm 647:
    Implementation and Relative Efficiency of Quasirandom
    Sequence Generators,
    ACM Transactions on Mathematical Software,
    Volume 12, Number 4, pages 362-376, 1986.
    P A Lewis, A S Goodman, J M Miller,
    A Pseudo-Random Number Generator for the System/360,
    IBM Systems Journal,
    Volume 8, pages 136-143, 1969.
  Parameters:
    Input/output, int *SEED, the "seed" value.  Normally, this
    value should not be 0.  On output, SEED has been updated.
    Output, double R8_UNIFORM_01, a new pseudorandom variate, strictly between
    0 and 1.
*/
double r8_uniform_01 (int *seed) {
  int k;
  double r;
  k = *seed/127773;
  *seed = 16807*(*seed - k*127773) - k*2836;
  if (*seed < 0) {
    *seed = *seed + 2147483647;
  }
  r = ((double)(*seed ))*4.656612875E-10;
  return r;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_CHOLESKY_FACTOR computes the Cholesky factor of a symmetric R8MAT.
  Discussion:
    An R8MAT is a doubly dimensioned array of R8 values, stored as a vector
    in column-major order.
    The matrix must be symmetric and positive semidefinite.
    For a positive semidefinite symmetric matrix A, the Cholesky factorization
    is a lower triangular matrix L such that:
      A = L*L'
  Modified:
    11 November 2012
  Author:
    John Burkardt
  Parameters:
    Input, int N, the number of rows and columns of the matrix A.
    Input, double A[N*N], the N by N matrix.
    Output, int *FLAG, an error flag.
    0, no error was detected.
    1, the matrix is not positive definite.
    2, the matrix is not nonnegative definite.
    Output, double R8MAT_CHOLESKY_FACTOR[N*N], the N by N lower triangular
    Cholesky factor.
*/
static double *r8mat_cholesky_factor (int n, double a[], int *flag) {
  int i, j, k;
  double *c, sum2, tol;
  *flag = 0;
  tol = sqrt(DBL_EPSILON);
  c = r8mat_copy_new (n, n, a);
  for (j=0; j < n; j++) {
    for (i=0; i < j; i++) {
      c[i + j*n] = 0.0;
    }
    for (i=j; i < n; i++) {
      sum2 = c[j + i*n];
      for (k=0; k < j; k++) {
      	/* sum2 -= c[j+k*n]*c[i+k*n]; */
        sum2 = fma(-c[j + k*n], c[i + k*n], sum2);  /* 6.4.10 improvement */
      }
      if (i == j) {
        if (0.0 < sum2){
          c[i + j*n] = sqrt(sum2);
        } else if (sum2 < - tol) {
          *flag = 2;
          return NULL;
        } else {
          *flag = 1;
          c[i + j*n] = 0.0;
        }
      } else {
        if (c[j + j*n] != 0.0) {
          c[i + j*n] = sum2/c[j + j*n];
        } else {
          c[i + j*n] = 0.0;
        }
      }
    }
  }
  return c;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_COPY_NEW copies one R8MAT to a "new" R8MAT.
  Discussion:
    An R8MAT is a doubly dimensioned array of R8 values, stored as a vector
    in column-major order.
  Modified:
    26 July 2008
  Parameters:
    Input, int M, N, the number of rows and columns.
    Input, double A1[M*N], the matrix to be copied.
    Output, double R8MAT_COPY_NEW[M*N], the copy of A1.
*/
static double *r8mat_copy_new (int m, int n, double a1[]) {
  double *a2;
  int i, j;
  a2 = (double *)malloc(m*n*sizeof(double));
  for (j=0; j < n; j++) {
    for (i=0; i < m; i++) {
      a2[i + j*m] = a1[i + j*m];
    }
  }
  return a2;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_IS_SYMMETRIC checks an R8MAT for symmetry.
  Discussion:
    An R8MAT is a matrix of double precision real values.
  Modified:
    10 November 2012
  Parameters:
    Input, int M, N, the order of the matrix.
    Input, double A[M*N], the matrix.
    Output, double RMAT_IS_SYMMETRIC, measures the
    Frobenius norm of (A - A' ), which would be zero if the matrix
    were exactly symmetric.
*/
double r8mat_is_symmetric (int m, int n, double a[]) {
  int i, j;
  double value;
  if (m != n) {
    value = HUGE_VAL;
    return value;
  }
  value = 0.0;
  for (j=0; j < n; j++) {
    for (i=0; i < m; i++) {
      value += tools_intpow(a[i + j*m] - a[j + i*m], 2);
    }
  }
  value = sqrt(value);
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_MAX returns the maximum entry of an R8MAT.
  Discussion:
    An R8MAT is a doubly dimensioned array of R8 values, stored as a vector
    in column-major order.
  Modified:
    21 May 2011
  Parameters:
    Input, int M, the number of rows in A.
    Input, int N, the number of columns in A.
    Input, double A[M*N], the M by N matrix.
    Output, double R8MAT_MAX, the maximum entry of A.
*/
double r8mat_max (int m, int n, double a[]) {
  int i, j;
  double value;
  value = a[0 + 0*m];
  for (j=0; j < n; j++) {
    for (i=0; i < m; i++) {
      if (value < a[i + j*m]) {
        value = a[i + j*m];
      }
    }
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_MIN returns the minimum entry of an R8MAT.
  Discussion:
    An R8MAT is a doubly dimensioned array of R8 values, stored as a vector
    in column-major order.
  Modified:
    21 May 2011
  Parameters:
    Input, int M, the number of rows in A.
    Input, int N, the number of columns in A.
    Input, double A[M*N], the M by N matrix.
    Output, double R8MAT_MIN, the minimum entry of A.
*/
double r8mat_min (int m, int n, double a[]) {
  int i, j;
  double value;
  value = a[0 + 0*m];
  for (j=0; j < n; j++) {
    for (i=0; i < m; i++) {
      if (a[i + j*m] < value) {
        value = a[i + j*m];
      }
    }
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_MM_NEW multiplies two matrices.
  Discussion:
    An R8MAT is a doubly dimensioned array of R8 values, stored as a vector
    in column-major order.
    For this routine, the result is returned as the function value.
  Modified:
    08 April 2009
  Parameters:
    Input, int N1, N2, N3, the order of the matrices.
    Input, double A[N1*N2], double B[N2*N3], the matrices to multiply.
    Output, double R8MAT_MM[N1*N3], the product matrix C = A*B.
*/
static double *r8mat_mm_new (int n1, int n2, int n3, double a[], double b[]) {
  double *c;
  int i, j, k;
  c = (double *)malloc(n1*n3*sizeof(double));
  for (i=0; i < n1; i++) {
    for (j=0; j < n3; j++) {
      c[i + j*n1] = 0.0;
      for (k=0; k < n2; k++) {
      	/* c[i+j*n1] = c[i+j*n1] + a[i+k*n1]*b[k+j*n2]; */
        c[i + j*n1] = fma(a[i + k*n1], b[k + j*n2], c[i + j*n1]);  /* 6.4.10 improvement */
      }
    }
  }
  return c;
}

/******************************************************************************/
/*
  Purpose:
    R8MAT_NORMAL_01_NEW returns a unit pseudonormal R8MAT.
  Modified:
    03 October 2005
  Reference:
    Paul Bratley, Bennett Fox, Linus Schrage,
    A Guide to Simulation,
    Springer Verlag, pages 201-202, 1983.
    Bennett Fox,
    Algorithm 647:
    Implementation and Relative Efficiency of Quasirandom
    Sequence Generators,
    ACM Transactions on Mathematical Software,
    Volume 12, Number 4, pages 362-376, 1986.
    Peter Lewis, Allen Goodman, James Miller,
    A Pseudo-Random Number Generator for the System/360,
    IBM Systems Journal,
    Volume 8, pages 136-143, 1969.
  Parameters:
    Input, int M, N, the number of rows and columns in the array.
    Input/output, int *SEED, the "seed" value, which should NOT be 0.
    On output, SEED has been updated.
    Output, double R8MAT_NORMAL_01_NEW[M*N], the array of pseudonormal values.
*/
static double *r8mat_normal_01_new (int m, int n, int *seed) {
  double *r;
  r = r8vec_normal_01_new(m*n, seed);
  return r;
}

/******************************************************************************/
/*
  Purpose:
    R8VEC_LINSPACE_NEW creates a vector of linearly spaced values.
  Discussion:
    An R8VEC is a vector of R8's.
    4 points evenly spaced between 0 and 12 will yield 0, 4, 8, 12.

    In other words, the interval is divided into N-1 even subintervals,
    and the endpoints of intervals are used as the points.
  Modified:
    29 March 2011
  Parameters:
    Input, int N, the number of entries in the vector.
    Input, double A, B, the first and last entries.
    Output, double R8VEC_LINSPACE_NEW[N], a vector of linearly spaced data.
*/
static double *r8vec_linspace_new (int n, double a, double b) {
  int i;
  double *x;
  x = (double *)malloc(n*sizeof(double));
  if (n == 1) {
    x[0] = (a + b)/2.0;
  } else {
    for (i=0; i < n; i++) {
      x[i] = ((double)(n - 1 - i)*a
            + (double)(        i)*b)
             /(double)(n - 1    );
    }
  }
  return x;
}

/******************************************************************************/
/*
  Purpose:
    R8VEC_MIN returns the value of the minimum element in a R8VEC.
  Modified:
    05 May 2006
  Parameters:
    Input, int N, the number of entries in the array.
    Input, double R8VEC[N], the array to be checked.
    Output, double R8VEC_MIN, the value of the minimum element.
*/
double r8vec_min (int n, double r8vec[]) {
  int i;
  double value;
  value = r8vec[0];
  for (i=1; i < n; i++) {
    if (r8vec[i] < value) {
      value = r8vec[i];
    }
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    R8VEC_NORMAL_01_NEW returns a unit pseudonormal R8VEC.
  Discussion:
    The standard normal probability distribution function (PDF) has
    mean 0 and standard deviation 1.
    This routine can generate a vector of values on one call.  It
    has the feature that it should provide the same results
    in the same order no matter how we break up the task.
    Before calling this routine, the user may call RANDOM_SEED
    in order to set the seed of the random number generator.
    The Box-Muller method is used, which is efficient, but
    generates an even number of values each time.  On any call
    to this routine, an even number of new values are generated.
    Depending on the situation, one value may be left over.
    In that case, it is saved for the next call.
  Modified:
    18 February 2012
  Parameters:
    Input, int N, the number of values desired.  If N is negative,
    then the code will flush its internal memory; in particular,
    if there is a saved value to be used on the next call, it is
    instead discarded.  This is useful if the user has reset the
    random number seed, for instance.
    Input/output, int *SEED, a seed for the random number generator.
    Output, double R8VEC_NORMAL_01_NEW[N], a sample of the standard normal PDF.
  Local parameters:
    Local, int MADE, records the number of values that have
    been computed.  On input with negative N, this value overwrites
    the return value of N, so the user can get an accounting of
    how much work has been done.
    Local, double R[N+1], is used to store some uniform random values.
    Its dimension is N+1, but really it is only needed to be the
    smallest even number greater than or equal to N.
    Local, int SAVED, is 0 or 1 depending on whether there is a
    single saved value left over from the previous call.
    Local, int X_LO, X_HI, records the range of entries of
    X that we need to compute.  This starts off as 1:N, but is adjusted
    if we have a saved value that can be immediately stored in X(1),
    and so on.
    Local, double Y, the value saved from the previous call, if
    SAVED is 1.
*/
static double *r8vec_normal_01_new (int n, int *seed) {
#define R8_PI 3.141592653589793
  int i, m, x_hi, x_lo;
  static int made = 0;
  static int saved = 0;
  double *r, *x;
  static double y = 0.0;
  x = (double *)malloc(n*sizeof(double));
  /* I'd like to allow the user to reset the internal data.
  But this won't work properly if we have a saved value Y.
  I'm making a crock option that allows the user to signal
  explicitly that any internal memory should be flushed,
  by passing in a negative value for N. */
  if (n < 0) {
    made = 0;
    saved = 0;
    y = 0.0;
    return NULL;
  } else if (n == 0) {
    return NULL;
  }
  /* Record the range of X we need to fill in. */
  x_lo = 1;
  x_hi = n;
  /* Use up the old value, if we have it. */
  if (saved == 1) {
    x[0] = y;
    saved = 0;
    x_lo = 2;
  }
  /* Maybe we don't need any more values. */
  if (x_hi - x_lo + 1 == 0) {
  }
  /* If we need just one new value, do that here to avoid null arrays. */
  else if (x_hi - x_lo + 1 == 1) {
    r = r8vec_uniform_01_new(2, seed);
    x[x_hi - 1] = sqrt(- 2.0*sun_log(r[0]))*sun_cos(2.0*R8_PI*r[1]);
    y =           sqrt(- 2.0*sun_log(r[0]))*sun_sin(2.0*R8_PI*r[1]);
    saved = 1;
    made = made + 2;
    free(r);
  }
  /* If we require an even number of values, that's easy. */
  else if (( x_hi - x_lo + 1) % 2 == 0) {
    m = (x_hi - x_lo + 1)/2;
    r = r8vec_uniform_01_new(2*m, seed);
    for (i=0; i <= 2*m - 2; i += 2) {
      x[x_lo + i - 1] = sqrt(-2.0*sun_log(r[i]))*sun_cos(2.0*R8_PI*r[i + 1]);
      x[x_lo + i    ] = sqrt(-2.0*sun_log(r[i]))*sun_sin(2.0*R8_PI*r[i + 1]);
    }
    made = made + x_hi - x_lo + 1;
    free(r);
  }
  /* If we require an odd number of values, we generate an even number,
     and handle the last pair specially, storing one in X(N), and
     saving the other for later. */
  else {
    x_hi -= 1;
    m = (x_hi - x_lo + 1)/2 + 1;
    r = r8vec_uniform_01_new (2*m, seed);
    for (i=0; i <= 2*m-4; i += 2) {
      x[x_lo + i - 1] = sqrt(-2.0*sun_log(r[i]))*sun_cos(2.0*R8_PI*r[i + 1]);
      x[x_lo + i  ]   = sqrt(-2.0*sun_log(r[i]))*sun_sin(2.0*R8_PI*r[i + 1]);
    }
    i = 2*m - 2;
    x[x_lo + i - 1] = sqrt(- 2.0*sun_log(r[i]))*sun_cos(2.0*R8_PI*r[i + 1]);
    y               = sqrt(- 2.0*sun_log(r[i]))*sun_sin(2.0*R8_PI*r[i + 1]);
    saved = 1;
    made += x_hi - x_lo + 2;
    free(r);
  }
  return x;
#undef R8_PI
}

/******************************************************************************/
/*
  Purpose:
    R8VEC_UNIFORM_01_NEW returns a unit pseudorandom R8VEC.
  Discussion:
    This routine implements the recursion
      seed = 16807*seed mod (2^31 - 1 )
      unif = seed/( 2^31 - 1 )
    The integer arithmetic never requires more than 32 bits,
    including a sign bit.
  Modified:
    19 August 2004
  Reference:
    Paul Bratley, Bennett Fox, Linus Schrage,
    A Guide to Simulation,
    Second Edition,
    Springer, 1987,
    ISBN: 0387964673,
    LC: QA76.9.C65.B73.
    Bennett Fox,
    Algorithm 647:
    Implementation and Relative Efficiency of Quasirandom
    Sequence Generators,
    ACM Transactions on Mathematical Software,
    Volume 12, Number 4, December 1986, pages 362-376.
    Pierre L'Ecuyer,
    Random Number Generation,
    in Handbook of Simulation,
    edited by Jerry Banks,
    Wiley, 1998,
    ISBN: 0471134031,
    LC: T57.62.H37.
    Peter Lewis, Allen Goodman, James Miller,
    A Pseudo-Random Number Generator for the System/360,
    IBM Systems Journal,
    Volume 8, Number 2, 1969, pages 136-143.
  Parameters:
    Input, int N, the number of entries in the vector.
    Input/output, int *SEED, a seed for the random number generator.
    Output, double R8VEC_UNIFORM_01_NEW[N], the vector of pseudorandom values.
*/
static double *r8vec_uniform_01_new (int n, int *seed) {
  int i, k;
  int i4_huge = 2147483647;
  double *r;
  if (*seed == 0) { return NULL; }
  r = (double *)malloc(n*sizeof(double));
  for (i=0; i < n; i++) {
    k = *seed/127773;
    *seed = 16807*(*seed - k*127773) - k*2836;
    if (*seed < 0) {
      *seed = *seed + i4_huge;
    }
    r[i] = (double)(*seed)*4.656612875E-10;
  }
  return r;
}

/******************************************************************************/
/*
  Purpose:
    SAMPLE_PATHS_CHOLESKY: sample paths for stationary correlation functions.
  Discussion:
    This method uses the Cholesky factorization of the correlation matrix.
  Modified:
    10 November 2012
  Parameters:
    Input, int N, the number of points on each path.
    Input, int N2, the number of paths.
    Input, double RHOMAX, the maximum value of RHO.
    Input, double RHO0, the correlation length.
    Input, double *CORRELATION (int n, double rho_vec[], double rho0),
    the name of the function which evaluates the correlation.
    Input/output, int *SEED, a seed for the random number
    generator.
    Output, double X[N*N2], the sample paths.
*/
double *sample_paths_cholesky (int n, int n2, double rhomax, double rho0,
  double *correlation (int n, double rho_vec[], double rho0), int *seed) {
  int flag, i, j, k;
  double *cor, *cor_vec, *l, *r, *rho_vec, rhomin, *x;
  /* Choose N equally spaced sample points from 0 to RHOMAX. */
  rhomin = 0.0;
  rho_vec = r8vec_linspace_new (n, rhomin, rhomax);
  /* Evaluate the correlation function. */
  cor_vec = correlation (n, rho_vec, rho0);
  /* Construct the correlation matrix;
  From the vector
    [ C(0), C(1), C(2), ... C(N-1) ]
  construct the vector
    [ C(N-1), ..., C(2), C(1), C(0), C(1), C(2), ...  C(N-1) ]
  Every row of the correlation matrix can be constructed by a subvector
  of this vector. */
  cor = (double *)malloc(n*n*sizeof(double));
  for (j=0; j < n; j++) {
    for (i=0; i < n; i++) {
      k = i4_wrap(j - i, 0, n - 1);
      cor[i + j*n] = cor_vec[k];
    }
  }
  /* Get the Cholesky factorization of COR: COR = L*L'. */
  l = r8mat_cholesky_factor(n, cor, &flag);
  /* The matrix might not be nonnegative definite. */
  if (flag == 2) return NULL;
  /* Compute a matrix of N by N2 normally distributed values. */
  r = r8mat_normal_01_new(n, n2, seed);
  /* Compute the sample path. */
  x = r8mat_mm_new(n, n, n2, l, r);
  xfreeall(cor, cor_vec, l, r, rho_vec);
  return x;
}

/******************************************************************************/
/*
  Purpose:
    SAMPLE_PATHS_EIGEN: sample paths for stationary correlation functions.
  Discussion:
    This method uses the eigen-decomposition of the correlation matrix.
  Modified:
    12 November 2012
  Parameters:
    Input, int N, the number of points on each path.
    Input, int N2, the number of paths.
    Input, double RHOMAX, the maximum value of RHO.
    Input, double RHO0, the correlation length.
    Input, double *CORRELATION (int n, double rho_vec[], double rho0),
    the name of the function which evaluates the correlation.
    Input/output, int *SEED, a seed for the random number
    generator.
    Output, double X[N*N2], the sample paths.
*/
double *sample_paths_eigen (int n, int n2, double rhomax, double rho0,
  double *correlation (int n, double rho_vec[], double rho0), int *seed) {
  double *c, *cor, *cor_vec, *d, *r, *rho_vec, *v, *w, *x, rhomin;
  int i, j, k;
  /* Choose N equally spaced sample points from 0 to RHOMAX. */
  rhomin = 0.0;
  rho_vec = r8vec_linspace_new(n, rhomin, rhomax);
  /* Evaluate the correlation function. */
  cor_vec = correlation(n, rho_vec, rho0);
  /* Construct the correlation matrix;
  From the vector
    [ C(0), C(1), C(2), ... C(N-1) ]
  construct the vector
    [ C(N-1), ..., C(2), C(1), C(0), C(1), C(2), ...  C(N-1) ]
  Every row of the correlation matrix can be constructed by a subvector
  of this vector. */
  cor = (double *)malloc(n*n*sizeof(double));
  for (j=0; j < n; j++) {
    for (i=0; i < n; i++) {
      k = i4_wrap(abs(i - j), 0, n - 1);
      cor[i + j*n] = cor_vec[k];
    }
  }
  /* Get the eigendecomposition of COR: COR = V*D*V'.
     Because COR is symmetric, V is orthogonal. */
  d = (double *)malloc(n*sizeof(double));
  w = (double *)malloc(n*sizeof(double));
  v = (double *)malloc(n*n*sizeof(double));
  tred2(n, cor, d, w, v);
  tql2(n, d, w, v);
  /* We assume COR is non-negative definite, and hence that there are no negative eigenvalues. */
  r8vec_min(n, d);
  for (i=0; i < n; i++) {
    d[i] = fMax(d[i], 0.0);
  }
  /* Compute the eigenvalues of the factor C. */
  for (i=0; i < n; i++) {
    d[i] = sqrt(d[i]);
  }
  /* Compute C, such that C'*C = COR. */
  c = (double *)malloc(n*n*sizeof(double));
  for (j=0; j < n; j++) {
    for (i=0; i < n; i++) {
      c[i + j*n] = 0.0;
      for (k=0; k < n; k++) {
      	/* c[i+j*n] = c[i+j*n] + d[k]*v[i+k*n]*v[j+k*n]; */
        c[i + j*n] = fma(d[k], v[i + k*n]*v[j + k*n], c[i + j*n]);  /* 6.4.10 improvement */
      }
    }
  }
  /* Compute N by N2 independent random normal values. */
  r = r8mat_normal_01_new(n, n2, seed);
  /* Multiply to get the variables X which have correlation COR. */
  x = r8mat_mm_new(n, n, n2, c, r);
  xfreeall(c, cor, cor_vec, d, r, rho_vec, v, w);
  return x;
}

/******************************************************************************/
/*
  Purpose:
    SAMPLE_PATHS2_CHOLESKY: sample paths for stationary correlation functions.
  Discussion:
    This method uses the Cholesky factorization of the correlation matrix.
  Modified:
    12 November 2012
  Parameters:
    Input, int N, the number of points on each path.
    Input, int N2, the number of paths.
    Input, double RHOMIN, RHOMAX, the range of RHO.
    Input, double RHO0, the correlation length.
    Input, double *CORRELATION2 (int m, int n, double s[], double t[],
    double rho0), the name of the function which evaluates the correlation.
    Input/output, int *SEED, a seed for the random number
    generator.
    Output, double X[N*N2], the sample paths.
*/
double *sample_paths2_cholesky (int n, int n2, double rhomin, double rhomax,
  double rho0, double *correlation2 (int m, int n, double s[], double t[],
  double rho0), int *seed) {
  double *cor, *l, *r, *s, *x;
  int flag;
  /* Choose N equally spaced sample points from RHOMIN to RHOMAX. */
  s = r8vec_linspace_new(n, rhomin, rhomax);
  /* Evaluate the correlation function. */
  cor = correlation2(n, n, s, s, rho0);
  /* Get the Cholesky factorization of COR: COR = L*L'. */
  l = r8mat_cholesky_factor(n, cor, &flag);
  /* The matrix might not be nonnegative definite. */
  if (flag == 2) { return NULL; }
  /* Compute a matrix of N by N2 normally distributed values. */
  r = r8mat_normal_01_new(n, n2, seed);
  /* Compute the sample path. */
  x = r8mat_mm_new(n, n, n2, l, r);
  xfreeall(cor, l, r, s);
  return x;
}

/******************************************************************************/
/*
  Purpose:
    SAMPLE_PATHS2_EIGEN: sample paths for stationary correlation functions.
  Discussion:
    This method uses the eigen-decomposition of the correlation matrix.
  Modified:
    12 November 2012
  Parameters:
    Input, int N, the number of points on each path.
    Input, int N2, the number of paths.
    Input, double RHOMIN, RHOMAX, the range of RHO.
    Input, double RHO0, the correlation length.
    Input, double *CORRELATION2 (int m, int n, double s[], double t[],
    double rho0), the name of the function which evaluates the correlation.
    Input/output, int *SEED, a seed for the random number
    generator.
    Output, double X[N*N2], the sample paths.
*/
double *sample_paths2_eigen (int n, int n2, double rhomin, double rhomax,
  double rho0, double *correlation2 (int m, int n, double s[], double t[],
  double rho0), int *seed) {
  double *c, *cor, *d, *r, *s, *v, *w, *x, dmin;
  int i, j, k;
  /* Choose N equally spaced sample points from RHOMIN to RHOMAX. */
  s = r8vec_linspace_new(n, rhomin, rhomax);
  /* Evaluate the correlation function. */
  cor = correlation2(n, n, s, s, rho0);
  /* Get the eigendecomposition of COR: COR = V*D*V'.
  Because COR is symmetric, V is orthogonal. */
  d = (double *)malloc(n*sizeof(double));
  w = (double *)malloc(n*sizeof(double));
  v = (double *)malloc(n*n*sizeof(double));
  tred2(n, cor, d, w, v);
  tql2(n, d, w, v);
  /* We assume COR is non-negative definite, and hence that there are no negative eigenvalues. */
  dmin = r8vec_min(n, d);
  if (dmin < - sqrt(DBL_EPSILON)) {
    printf ("\n");
    printf ("SAMPLE_PATHS2_EIGEN - Warning!\n");
    printf ("  Negative eigenvalues observed as low as %g\n", dmin);
  }
  for (i=0; i < n; i++) {
    d[i] = fMax(d[i], 0.0);
  }
  /* Compute the eigenvalues of the factor C. */
  for (i=0; i < n; i++) {
    d[i] = sqrt(d[i]);
  }
  /* Compute C, such that C'*C = COR. */
  c = (double *)malloc(n*n*sizeof(double));
  for (j=0; j < n; j++) {
    for (i=0; i < n; i++) {
      c[i + j*n] = 0.0;
      for (k=0; k < n; k++) {
      	/* c[i + j*n] = c[i+j*n] + d[k]*v[i + k*n]*v[j + k*n]; */
        c[i + j*n] = fma(d[k], v[i + k*n]*v[j + k*n], c[i + j*n]);  /* 6.4.10 improvement */
      }
    }
  }
  /* Compute N by N2 independent random normal values. */
  r = r8mat_normal_01_new(n, n2, seed);
  /* Multiply to get the variables X which have correlation COR. */
  x = r8mat_mm_new(n, n, n2, c, r);
  xfreeall(c, cor, d, r, s, v, w);
  return x;
}

/******************************************************************************/
/*
  Purpose:
    TQL2 computes all eigenvalues/vectors, real symmetric tridiagonal matrix.
  Discussion:
    This subroutine finds the eigenvalues and eigenvectors of a symmetric
    tridiagonal matrix by the QL method.  The eigenvectors of a full
    symmetric matrix can also be found if TRED2 has been used to reduce this
    full matrix to tridiagonal form.
  Modified:
    08 November 2012
  Author:
    Original FORTRAN77 version by Smith, Boyle, Dongarra, Garbow, Ikebe,
    Klema, Moler.
    C version by John Burkardt.
  Reference:
    Bowdler, Martin, Reinsch, Wilkinson,
    TQL2,
    Numerische Mathematik,
    Volume 11, pages 293-306, 1968.
    James Wilkinson, Christian Reinsch,
    Handbook for Automatic Computation,
    Volume II, Linear Algebra, Part 2,
    Springer, 1971,
    ISBN: 0387054146,
    LC: QA251.W67.
    Brian Smith, James Boyle, Jack Dongarra, Burton Garbow,
    Yasuhiko Ikebe, Virginia Klema, Cleve Moler,
    Matrix Eigensystem Routines, EISPACK Guide,
    Lecture Notes in Computer Science, Volume 6,
    Springer Verlag, 1976,
    ISBN13: 978-3540075462,
    LC: QA193.M37.
  Parameters:
    Input, int N, the order of the matrix.
    Input/output, double D[N].  On input, the diagonal elements of
    the matrix.  On output, the eigenvalues in ascending order.  If an error
    exit is made, the eigenvalues are correct but unordered for indices
    1,2,...,IERR-1.
    Input/output, double E[N].  On input, E(2:N) contains the
    subdiagonal elements of the input matrix, and E(1) is arbitrary.
    On output, E has been destroyed.
    Input, double Z[N*N].  On input, the transformation matrix
    produced in the reduction by TRED2, if performed.  If the eigenvectors of
    the tridiagonal matrix are desired, Z must contain the identity matrix.
    On output, Z contains the orthonormal eigenvectors of the symmetric
    tridiagonal (or full) matrix.  If an error exit is made, Z contains
    the eigenvectors associated with the stored eigenvalues.
    Output, int TQL2, error flag.
    0, normal return,
    J, if the J-th eigenvalue has not been determined after
    30 iterations.
*/
static int tql2 (int n, double d[], double e[], double z[]) {
  double c, c2, c3, dl1, el1, f, g, h, p, r, s, s2, t, tst1, tst2, tmp;
  int i, ierr, ii, j, k, l, l1, l2, m, mml;
  ierr = 0;
  if (n == 1) return ierr;
  for (i=1; i < n; i++) e[i-1] = e[i];
  f = 0.0;
  tst1 = 0.0;
  e[n - 1] = 0.0;
  for (l = 0; l < n; l++) {
    j = 0;
    h = fabs(d[l]) + fabs(e[l]);
    tst1 = fMax(tst1, h);
    /* Look for a small sub-diagonal element. */
    for (m = l; m < n; m++) {
      tst2 = tst1 + fabs(e[m]);
      if (tst2 == tst1) {
        break;
      }
    }
    if (m != l ) {
      for (; ;) {
        if (30 <= j ) {
          ierr = l + 1;
          return ierr;
        }
        j = j + 1;
        /* Form shift. */
        l1 = l + 1;
        l2 = l1 + 1;
        g = d[l];
        p = (d[l1] - g)/(2.0*e[l]);
        r = sun_hypot(p, 1.0);
        tmp = p + tools_signum(p)*fabs(r);
        d[l]  = e[l]/tmp;
        d[l1] = e[l]*tmp;
        dl1 = d[l1];
        h = g - d[l];
        for (i=l2; i < n; i++) {
          d[i] -= h;
        }
        f += h;
        /* QL transformation. */
        p = d[m];
        c = c3 = 1.0;
        c2 = c;
        el1 = e[l1];
        s = s2 = 0.0;
        mml = m - l;
        for (ii = 1; ii <= mml; ii++) {
          c3 = c2;
          c2 = c;
          s2 = s;
          i = m - ii;
          g = c*e[i];
          h = c*p;
          r = sun_hypot(p, e[i]);
          e[i + 1] = s*r;
          s = e[i]/r;
          c = p/r;
          p = c*d[i] - s*g;
          /* d[i+1] = h + s*(c*g + s*d[i]); */
          d[i + 1] = fma(s, fma(c, g, s*d[i]), h);  /* 6.4.10 improvement */
          /* Form vector. */
          for (k=0; k < n; k++) {
            h = z[k + (i + 1)*n];
            /* z[k+(i+1)*n] = s*z[k+i*n] + c*h; */
            z[k + (i + 1)*n] = fma(s, z[k + i*n], c*h);  /* 6.4.10 improvement */
            /* z[k + i*n] = c*z[k + i*n] - s*h; */
            z[k + i*n] = fma(c, z[k + i*n], -s*h);  /* 6.4.10 improvement */
          }
        }
        p = -s*s2*c3*el1*e[l]/dl1;
        e[l] = s*p;
        d[l] = c*p;
        tst2 = tst1 + fabs(e[l]);
        if (tst2 <= tst1) break;
      }
    }
    d[l] = d[l] + f;
  }
  /* Order eigenvalues and eigenvectors. */
  for (ii=1; ii < n; ii++) {
    i = ii - 1;
    k = i;
    p = d[i];
    for (j = ii; j < n; j++) {
      if (d[j] < p) {
        k = j;
        p = d[j];
      }
    }
    if (k != i) {
      d[k] = d[i];
      d[i] = p;
      for (j=0; j < n; j++) {
        t          = z[j + i*n];
        z[j + i*n] = z[j + k*n];
        z[j + k*n] = t;
      }
    }
  }
  return ierr;
}

/******************************************************************************/
/*
  Purpose:
    TRED2 transforms a real symmetric matrix to symmetric tridiagonal form.
  Discussion:
    This subroutine reduces a real symmetric matrix to a
    symmetric tridiagonal matrix using and accumulating
    orthogonal similarity transformations.
    A and Z may coincide, in which case a single storage area is used
    for the input of A and the output of Z.
  Modified:
    03 November 2012
  Author:
    Original FORTRAN77 version by Smith, Boyle, Dongarra, Garbow, Ikebe,
    Klema, Moler.
    C version by John Burkardt.
  Reference:
    Martin, Reinsch, Wilkinson,
    TRED2,
    Numerische Mathematik,
    Volume 11, pages 181-195, 1968.
    James Wilkinson, Christian Reinsch,
    Handbook for Automatic Computation,
    Volume II, Linear Algebra, Part 2,
    Springer, 1971,
    ISBN: 0387054146,
    LC: QA251.W67.
    Brian Smith, James Boyle, Jack Dongarra, Burton Garbow,
    Yasuhiko Ikebe, Virginia Klema, Cleve Moler,
    Matrix Eigensystem Routines, EISPACK Guide,
    Lecture Notes in Computer Science, Volume 6,
    Springer Verlag, 1976,
    ISBN13: 978-3540075462,
    LC: QA193.M37.
  Parameters:
    Input, int N, the order of the matrix.
    Input, double A[N*N], the real symmetric input matrix.  Only the
    lower triangle of the matrix need be supplied.
    Output, double D[N], the diagonal elements of the tridiagonal
    matrix.
    Output, double E[N], contains the subdiagonal elements of the
    tridiagonal matrix in E(2:N).  E(1) is set to zero.
    Output, double Z[N*N], the orthogonal transformation matrix
    produced in the reduction.
*/
static void tred2 (int n, double a[], double d[], double e[], double z[]) {
  double f, g, h, hh, scale;
  int i, j, k,  l;
  for (j=0; j < n; j++) {
    for (i=j; i < n; i++) {
      z[i + j*n] = a[i + j*n];
    }
  }
  for (j=0; j < n; j++) {
    d[j] = a[n - 1 + j*n];
  }
  for (i=n - 1; 1 <= i; i--) {
    l = i - 1;
    h = 0.0;
    /* Scale row. */
    scale = 0.0;
    for (k=0; k <= l; k++) {
      scale = scale + fabs(d[k]);
    }
    if (scale == 0.0) {
      e[i] = d[l];
      for (j=0; j <= l; j++) {
        d[j]       = z[l + j*n];
        z[i + j*n] = 0.0;
        z[j + i*n] = 0.0;
      }
      d[i] = 0.0;
      continue;
    }
    for (k=0; k <= l; k++) {
      d[k] /= scale;
    }
    h = 0.0;
    for (k=0; k <= l; k++) {
      /* h = h + d[k]*d[k]; */
      h = fma(d[k], d[k], h);  /* 6.4.10 improvement */
    }
    f = d[l];
    g = - sqrt(h)*tools_signum(f);
    e[i] = scale*g;
    h -= f*g;
    d[l] = f - g;
    /* Form A*U. */
    for (k=0; k <= l; k++) {
      e[k] = 0.0;
    }
    for (j=0; j <= l; j++) {
      f = d[j];
      z[j + i*n] = f;
      /* g = e[j] + z[j+j*n]*f; */
      g = fma(z[j + j*n], f, e[j]);  /* 6.4.10 improvement */
      for (k=j + 1; k <= l; k++) {
      	/* g = g + z[k+j*n]*d[k]; */
        g =    fma(z[k + j*n], d[k], g);  /* 6.4.10 improvement */
        /* e[k] = e[k] + z[k+j*n]*f; */
        e[k] = fma(z[k + j*n], f, e[k]);  /* 6.4.10 improvement */
      }
      e[j] = g;
    }
    /* Form P. */
    for (k=0; k <= l; k++) {
      e[k] /= h;
    }
    f = 0.0;
    for (k=0; k <= l; k++) {
      /* f = f + e[k]*d[k]; */
      f = fma(e[k], d[k], f);  /* 6.4.10 improvement */
    }
    hh = 0.5*f/h;
    /* Form Q. */
    for (k=0; k <= l; k++) {
      /* e[k] = e[k] - hh*d[k]; */
      e[k] = fma(-hh, d[k], e[k]);  /* 6.4.10 improvement */
    }
    /* Form reduced A. */
    for (j=0; j <= l; j++) {
      f = d[j];
      g = e[j];
      for (k=j; k <= l; k++) {
        z[k + j*n] = fma(-f, e[k], fma(-g, d[k], z[k + j*n]));  /* 6.4.10 improvement */
      }
      d[j] = z[l + j*n];
      z[i + j*n] = 0.0;
    }
    d[i] = h;
  }
  /* Accumulation of transformation matrices. */
  for (i=1; i < n; i++) {
    l = i - 1;
    z[n - 1 + l*n] = z[l + l*n];
    z[l + l*n] = 1.0;
    h = d[i];
    if (h != 0.0) {
      for (k=0; k <= l; k++) {
        d[k] = z[k + i*n]/h;
      }
      for (j=0; j <= l; j++) {
        g = 0.0;
        for (k=0; k <= l; k++) {
          /* g = g + z[k+i*n]*z[k+j*n]; */
          g = fma(z[k + i*n], z[k + j*n], g);  /* 6.4.10 improvement */
        }
        for (k=0; k <= l; k++) {
          /* z[k+j*n] = z[k+j*n] - g*d[k]; */
          z[k + j*n] = fma(-g, d[k], z[k + j*n]);  /* 6.4.10 improvement */
        }
      }
    }
    for (k = 0; k <= l; k++) {
      z[k + i*n] = 0.0;
    }
  }
  for (j=0; j < n; j++) {
    d[j] = z[n - 1 + j*n];
  }
  for (j=0; j < n - 1; j++) {
    z[n - 1 + j*n] = 0.0;
  }
  z[n - 1 + (n - 1)*n] = 1.0;
  e[0] = 0.0;
  return;
}

/******************************************************************************/
/*
  Purpose:
    I4_MODP returns the nonnegative remainder of I4 division.
  Discussion:
    If
      NREM = I4_MODP (I, J )
      NMULT = (I - NREM)/J
    then
      I = J*NMULT + NREM
    where NREM is always nonnegative.
    The MOD function computes a result with the same sign as the
    quantity being divided.  Thus, suppose you had an angle A,
    and you wanted to ensure that it was between 0 and 360.
    Then mod(A,360) would do, if A was positive, but if A
    was negative, your result would be between -360 and 0.
    On the other hand, I4_MODP(A,360) is between 0 and 360, always.
  Example:
        I         J     MOD  I4_MODP   I4_MODP Factorization
      107        50       7       7    107 =  2* 50 + 7
      107       -50       7       7    107 = -2*-50 + 7
     -107        50      -7      43   -107 = -3* 50 + 43
     -107       -50      -7      43   -107 =  3*-50 + 43
  Licensing:
    This code is distributed under the MIT license.
  Modified:
    12 January 2007
  Author:
    John Burkardt
  Parameters:
    Input, int I, the number to be divided.
    Input, int J, the number that divides I.
    Output, int I4_MODP, the nonnegative remainder when I is
    divided by J.
*/
int i4_modp (int i, int j, int *rc) {
  int value;
  if (j == 0) { *rc = 1; return 0; }
  *rc = 0;
  value = i % j;
  if (value < 0) {
    value += abs(j);
  }
  return value;
}

/******************************************************************************/
/*
  Purpose:
    I4_WRAP forces an I4 to lie between given limits by wrapping.
  Example:
    ILO = 4, IHI = 8
    I   Value
    -2     8
    -1     4
     0     5
     1     6
     2     7
     3     8
     4     4
     5     5
     6     6
     7     7
     8     8
     9     4
    10     5
    11     6
    12     7
    13     8
    14     4
  Licensing:
    This code is distributed under the MIT license.
  Modified:
    17 July 2008
  Author:
    John Burkardt
  Parameters:
    Input, int IVAL, an integer value.
    Input, int ILO, IHI, the desired bounds for the integer value.
    Output, int I4_WRAP, a "wrapped" version of IVAL.
*/
int i4_wrap (int ival, int ilo, int ihi) {
  int jhi, jlo, wide, rc;
  jlo = fMin(ilo, ihi);
  jhi = fMax(ilo, ihi);
  wide = jhi + 1 - jlo;
  return (wide == 1) ? jlo : jlo + i4_modp(ival - jlo, wide, &rc);
}


/*****************************************************************************/
/*
  Purpose:
    lambert_w() approximates the Lambert W function.
  Discussion:
    The call will fail if the input value X is out of range.
    The range requirement for the upper branch is:
      -exp(-1) <= X.
    The range requirement for the lower branch is:
      -exp(-1) < X < 0.
  Licensing:
    This code is distributed under the MIT license.
  Modified:
    21 June 2023
  Author:
    Original FORTRAN77 version by Andrew Barry, S. J. Barry,
    Patricia Culligan-Hensley.
    This version by John Burkardt.
  Reference:
    Andrew Barry, S. J. Barry, Patricia Culligan-Hensley,
    Algorithm 743: WAPR - A Fortran routine for calculating real
    values of the W-function,
    ACM Transactions on Mathematical Software,
    Volume 21, Number 2, June 1995, pages 172-181.
  Input:
     double x: the argument.
     int nb: indicates the desired branch.
     * 0, the upper branch;
     * nonzero, the lower branch.
     int l: indicates the interpretation of X.
     * 1, X is actually the offset from -(exp-1), so compute W(X-exp(-1)).
     * not 1, X is the argument; compute W(X);
  Output:
     double lambert_w: the approximate value of W(X).
*/
double r8_lambert_w (double x, int nb, int l) {
  double an2, an3, an4, an5, an6, c13, c23, d12, delx, em, em2, em9, eta, reta,
    s2, s21, s22, s23, t, tb, temp, temp2, ts, value, x0, x1, xx, zl, zn;
  int i, nbits, niter;
  value = 0.0;
  niter = 1;
  nbits = 52;
  /* various mathematical constants */
  em = -sun_exp(-1.0);
  em9 = -sun_exp(-9.0);
  c13 = 1.0/3.0;
  c23 = 2.0*c13;
  em2 = 2.0/em;
  d12 = -em2;
  tb = sun_pow(0.5, nbits, 1);
  x0 = sun_pow(tb, 1.0/6.0, 1)*0.5;
  x1 = (1.0 - 17.0*sun_pow(tb, 2.0/7.0, 1))*em;
  an3 = 8.0/3.0;
  an4 = 135.0/83.0;
  an5 = 166.0/39.0;
  an6 = 3167.0/3549.0;
  s2 = sqrt(2.0);
  s21 = 2.0*s2 - 3.0;
  s22 = 4.0 - 3.0*s2;
  s23 = s2 - 2.0;
  if (l == 1) {
    delx = x;
    if (delx < 0.0) return AGN_NAN;
    xx = x + em;
  } else {
    if (x < em) return AGN_NAN;
    else if (x == em) {
      value = -1.0;
      return value;
    }
    xx = x;
    delx = xx - em;
  }
  /* calculations for Wp. */
  if (nb == 0) {
    if (fabs(xx) <= x0) {
      value = xx/(1.0 + xx/(1.0 + xx/(2.0 + xx/(0.6 + 0.34*xx))));
      return value;
    } else if (xx <= x1) {
      reta = sqrt(d12*delx);
      value = reta/(1.0 + reta/(3.0 + reta/(reta/(an4 + reta/(reta*an6 + an5)) + an3))) - 1.0;
      return value;
    } else if (xx <= 20.0) {
      reta = s2*sqrt(1.0 - xx/em);
      an2 = 4.612634277343749*sqrt(sqrt(reta +  1.09556884765625));
      value = reta/(1.0 + reta/(3.0 + (s21*an2 + s22)*reta/(s23*(an2 + reta)))) - 1.0;
    } else {
      zl = sun_log(xx);
      value = sun_log(xx/sun_log(xx
       /sun_pow(zl, sun_exp(-1.124491989777808/(0.4225028202459761 + zl)), 1)));
    }
  } else  /* calculations for Wm. */ {
    if (0.0 <= xx) return AGN_NAN;
    if (xx <= x1) {
      reta = sqrt(d12*delx);
      value = reta/(reta/(3.0 + reta/(reta/(an4 + reta/(reta*an6 - an5)) - an3)) - 1.0) - 1.0;
      return value;
    }
    else if (xx <= em9) {
      zl = sun_log(-xx);
      t = -1.0 - zl;
      ts = sqrt(t);
      /* value = zl - (2.0*ts)/(s2 + (c13 - t/(270.0 + ts*127.0471381349219))*ts); */
      value = zl - (2.0*ts)/(s2 + (c13 - t/(fma(ts, 127.0471381349219, 270.0)))*ts);  /* 6.4.10 improvement */
    } else {
      zl = sun_log(-xx);
      eta = 2.0 - em2*xx;
      /* value = sun_log(xx/sun_log(-xx/((1.0 - 0.5043921323068457*(zl + 1.0))
       *(sqrt(eta) + eta/3.0) + 1.0))); */
      value = sun_log(xx/sun_log(-xx/((fma(-0.5043921323068457, zl + 1.0, 1.0))
        *(sqrt(eta) + eta/3.0) + 1.0)));  /* 6.4.10 improvement */
    }
  }
  for (i=1; i <= niter; i++) {
    zn = sun_log(xx/value) - value;
    temp = 1.0 + value;
    temp2 = temp + c23*zn;
    temp2 *= 2.0*temp;
    value *= 1.0 + (zn/temp)*(temp2 - zn)/(temp2 - 2.0*zn);
  }
  return value;
}


/******************************************************************************/
/*
  Purpose:
    R8_E1 evaluates the exponential integral E1 for an R8 argument.

  Licensing:
    This code is distributed under the MIT license.

  Modified:
    17 January 2012

  Author:
    Original FORTRAN77 version by Wayne Fullerton.
    C version by John Burkardt.

  Reference:
    Wayne Fullerton,
    Portable Special Function Routines,
    in Portability of Numerical Software,
    edited by Wayne Cowell,
    Lecture Notes in Computer Science, Volume 57,
    Springer 1977,
    ISBN: 978-3-540-08446-4,
    LC: QA297.W65.

  Parameters:
    Input, double X, the argument.
    Output, double R8_E1, the exponential integral E1 evaluated at X.
*/
double r8_e1 (double x) {
  static double ae10cs[50] = {
    +0.3284394579616699087873844201881E-01,
    -0.1669920452031362851476184343387E-01,
    +0.2845284724361346807424899853252E-03,
    -0.7563944358516206489487866938533E-05,
    +0.2798971289450859157504843180879E-06,
    -0.1357901828534531069525563926255E-07,
    +0.8343596202040469255856102904906E-09,
    -0.6370971727640248438275242988532E-10,
    +0.6007247608811861235760831561584E-11,
    -0.7022876174679773590750626150088E-12,
    +0.1018302673703687693096652346883E-12,
    -0.1761812903430880040406309966422E-13,
    +0.3250828614235360694244030353877E-14,
    -0.5071770025505818678824872259044E-15,
    +0.1665177387043294298172486084156E-16,
    +0.3166753890797514400677003536555E-16,
    -0.1588403763664141515133118343538E-16,
    +0.4175513256138018833003034618484E-17,
    -0.2892347749707141906710714478852E-18,
    -0.2800625903396608103506340589669E-18,
    +0.1322938639539270903707580023781E-18,
    -0.1804447444177301627283887833557E-19,
    -0.7905384086522616076291644817604E-20,
    +0.4435711366369570103946235838027E-20,
    -0.4264103994978120868865309206555E-21,
    -0.3920101766937117541553713162048E-21,
    +0.1527378051343994266343752326971E-21,
    +0.1024849527049372339310308783117E-22,
    -0.2134907874771433576262711405882E-22,
    +0.3239139475160028267061694700366E-23,
    +0.2142183762299889954762643168296E-23,
    -0.8234609419601018414700348082312E-24,
    -0.1524652829645809479613694401140E-24,
    +0.1378208282460639134668480364325E-24,
    +0.2131311202833947879523224999253E-26,
    -0.2012649651526484121817466763127E-25,
    +0.1995535662263358016106311782673E-26,
    +0.2798995808984003464948686520319E-26,
    -0.5534511845389626637640819277823E-27,
    -0.3884995396159968861682544026146E-27,
    +0.1121304434507359382850680354679E-27,
    +0.5566568152423740948256563833514E-28,
    -0.2045482929810499700448533938176E-28,
    -0.8453813992712336233411457493674E-29,
    +0.3565758433431291562816111116287E-29,
    +0.1383653872125634705539949098871E-29,
    -0.6062167864451372436584533764778E-30,
    -0.2447198043989313267437655119189E-30,
    +0.1006850640933998348011548180480E-30,
    +0.4623685555014869015664341461674E-31 };
  static double ae11cs[60] = {
    +0.20263150647078889499401236517381,
    -0.73655140991203130439536898728034E-01,
    +0.63909349118361915862753283840020E-02,
    -0.60797252705247911780653153363999E-03,
    -0.73706498620176629330681411493484E-04,
    +0.48732857449450183453464992488076E-04,
    -0.23837064840448290766588489460235E-05,
    -0.30518612628561521027027332246121E-05,
    +0.17050331572564559009688032992907E-06,
    +0.23834204527487747258601598136403E-06,
    +0.10781772556163166562596872364020E-07,
    -0.17955692847399102653642691446599E-07,
    -0.41284072341950457727912394640436E-08,
    +0.68622148588631968618346844526664E-09,
    +0.53130183120506356147602009675961E-09,
    +0.78796880261490694831305022893515E-10,
    -0.26261762329356522290341675271232E-10,
    -0.15483687636308261963125756294100E-10,
    -0.25818962377261390492802405122591E-11,
    +0.59542879191591072658903529959352E-12,
    +0.46451400387681525833784919321405E-12,
    +0.11557855023255861496288006203731E-12,
    -0.10475236870835799012317547189670E-14,
    -0.11896653502709004368104489260929E-13,
    -0.47749077490261778752643019349950E-14,
    -0.81077649615772777976249734754135E-15,
    +0.13435569250031554199376987998178E-15,
    +0.14134530022913106260248873881287E-15,
    +0.49451592573953173115520663232883E-16,
    +0.79884048480080665648858587399367E-17,
    -0.14008632188089809829248711935393E-17,
    -0.14814246958417372107722804001680E-17,
    -0.55826173646025601904010693937113E-18,
    -0.11442074542191647264783072544598E-18,
    +0.25371823879566853500524018479923E-20,
    +0.13205328154805359813278863389097E-19,
    +0.62930261081586809166287426789485E-20,
    +0.17688270424882713734999261332548E-20,
    +0.23266187985146045209674296887432E-21,
    -0.67803060811125233043773831844113E-22,
    -0.59440876959676373802874150531891E-22,
    -0.23618214531184415968532592503466E-22,
    -0.60214499724601478214168478744576E-23,
    -0.65517906474348299071370444144639E-24,
    +0.29388755297497724587042038699349E-24,
    +0.22601606200642115173215728758510E-24,
    +0.89534369245958628745091206873087E-25,
    +0.24015923471098457555772067457706E-25,
    +0.34118376888907172955666423043413E-26,
    -0.71617071694630342052355013345279E-27,
    -0.75620390659281725157928651980799E-27,
    -0.33774612157467324637952920780800E-27,
    -0.10479325703300941711526430332245E-27,
    -0.21654550252170342240854880201386E-28,
    -0.75297125745288269994689298432000E-30,
    +0.19103179392798935768638084000426E-29,
    +0.11492104966530338547790728833706E-29,
    +0.43896970582661751514410359193600E-30,
    +0.12320883239205686471647157725866E-30,
    +0.22220174457553175317538581162666E-31 };
  static double ae12cs[41] = {
    +0.63629589796747038767129887806803,
    -0.13081168675067634385812671121135,
    -0.84367410213053930014487662129752E-02,
    +0.26568491531006685413029428068906E-02,
    +0.32822721781658133778792170142517E-03,
    -0.23783447771430248269579807851050E-04,
    -0.11439804308100055514447076797047E-04,
    -0.14405943433238338455239717699323E-05,
    +0.52415956651148829963772818061664E-08,
    +0.38407306407844323480979203059716E-07,
    +0.85880244860267195879660515759344E-08,
    +0.10219226625855003286339969553911E-08,
    +0.21749132323289724542821339805992E-10,
    -0.22090238142623144809523503811741E-10,
    -0.63457533544928753294383622208801E-11,
    -0.10837746566857661115340539732919E-11,
    -0.11909822872222586730262200440277E-12,
    -0.28438682389265590299508766008661E-14,
    +0.25080327026686769668587195487546E-14,
    +0.78729641528559842431597726421265E-15,
    +0.15475066347785217148484334637329E-15,
    +0.22575322831665075055272608197290E-16,
    +0.22233352867266608760281380836693E-17,
    +0.16967819563544153513464194662399E-19,
    -0.57608316255947682105310087304533E-19,
    -0.17591235774646878055625369408853E-19,
    -0.36286056375103174394755328682666E-20,
    -0.59235569797328991652558143488000E-21,
    -0.76030380926310191114429136895999E-22,
    -0.62547843521711763842641428479999E-23,
    +0.25483360759307648606037606400000E-24,
    +0.25598615731739857020168874666666E-24,
    +0.71376239357899318800207052800000E-25,
    +0.14703759939567568181578956800000E-25,
    +0.25105524765386733555198634666666E-26,
    +0.35886666387790890886583637333333E-27,
    +0.39886035156771301763317759999999E-28,
    +0.21763676947356220478805333333333E-29,
    -0.46146998487618942367607466666666E-30,
    -0.20713517877481987707153066666666E-30,
    -0.51890378563534371596970666666666E-31 };
  static double ae13cs[50] = {
    -0.60577324664060345999319382737747,
    -0.11253524348366090030649768852718,
    +0.13432266247902779492487859329414E-01,
    -0.19268451873811457249246838991303E-02,
    +0.30911833772060318335586737475368E-03,
    -0.53564132129618418776393559795147E-04,
    +0.98278128802474923952491882717237E-05,
    -0.18853689849165182826902891938910E-05,
    +0.37494319356894735406964042190531E-06,
    -0.76823455870552639273733465680556E-07,
    +0.16143270567198777552956300060868E-07,
    -0.34668022114907354566309060226027E-08,
    +0.75875420919036277572889747054114E-09,
    -0.16886433329881412573514526636703E-09,
    +0.38145706749552265682804250927272E-10,
    -0.87330266324446292706851718272334E-11,
    +0.20236728645867960961794311064330E-11,
    -0.47413283039555834655210340820160E-12,
    +0.11221172048389864324731799928920E-12,
    -0.26804225434840309912826809093395E-13,
    +0.64578514417716530343580369067212E-14,
    -0.15682760501666478830305702849194E-14,
    +0.38367865399315404861821516441408E-15,
    -0.94517173027579130478871048932556E-16,
    +0.23434812288949573293896666439133E-16,
    -0.58458661580214714576123194419882E-17,
    +0.14666229867947778605873617419195E-17,
    -0.36993923476444472706592538274474E-18,
    +0.93790159936721242136014291817813E-19,
    -0.23893673221937873136308224087381E-19,
    +0.61150624629497608051934223837866E-20,
    -0.15718585327554025507719853288106E-20,
    +0.40572387285585397769519294491306E-21,
    -0.10514026554738034990566367122773E-21,
    +0.27349664930638667785806003131733E-22,
    -0.71401604080205796099355574271999E-23,
    +0.18705552432235079986756924211199E-23,
    -0.49167468166870480520478020949333E-24,
    +0.12964988119684031730916087125333E-24,
    -0.34292515688362864461623940437333E-25,
    +0.90972241643887034329104820906666E-26,
    -0.24202112314316856489934847999999E-26,
    +0.64563612934639510757670475093333E-27,
    -0.17269132735340541122315987626666E-27,
    +0.46308611659151500715194231466666E-28,
    -0.12448703637214131241755170133333E-28,
    +0.33544574090520678532907007999999E-29,
    -0.90598868521070774437543935999999E-30,
    +0.24524147051474238587273216000000E-30,
    -0.66528178733552062817107967999999E-31 };
  static double ae14cs[64] = {
    -0.1892918000753016825495679942820,
    -0.8648117855259871489968817056824E-01,
    +0.7224101543746594747021514839184E-02,
    -0.8097559457557386197159655610181E-03,
    +0.1099913443266138867179251157002E-03,
    -0.1717332998937767371495358814487E-04,
    +0.2985627514479283322825342495003E-05,
    -0.5659649145771930056560167267155E-06,
    +0.1152680839714140019226583501663E-06,
    -0.2495030440269338228842128765065E-07,
    +0.5692324201833754367039370368140E-08,
    -0.1359957664805600338490030939176E-08,
    +0.3384662888760884590184512925859E-09,
    -0.8737853904474681952350849316580E-10,
    +0.2331588663222659718612613400470E-10,
    -0.6411481049213785969753165196326E-11,
    +0.1812246980204816433384359484682E-11,
    -0.5253831761558460688819403840466E-12,
    +0.1559218272591925698855028609825E-12,
    -0.4729168297080398718476429369466E-13,
    +0.1463761864393243502076199493808E-13,
    -0.4617388988712924102232173623604E-14,
    +0.1482710348289369323789239660371E-14,
    -0.4841672496239229146973165734417E-15,
    +0.1606215575700290408116571966188E-15,
    -0.5408917538957170947895023784252E-16,
    +0.1847470159346897881370231402310E-16,
    -0.6395830792759094470500610425050E-17,
    +0.2242780721699759457250233276170E-17,
    -0.7961369173983947552744555308646E-18,
    +0.2859308111540197459808619929272E-18,
    -0.1038450244701137145900697137446E-18,
    +0.3812040607097975780866841008319E-19,
    -0.1413795417717200768717562723696E-19,
    +0.5295367865182740958305442594815E-20,
    -0.2002264245026825902137211131439E-20,
    +0.7640262751275196014736848610918E-21,
    -0.2941119006868787883311263523362E-21,
    +0.1141823539078927193037691483586E-21,
    -0.4469308475955298425247020718489E-22,
    +0.1763262410571750770630491408520E-22,
    -0.7009968187925902356351518262340E-23,
    +0.2807573556558378922287757507515E-23,
    -0.1132560944981086432141888891562E-23,
    +0.4600574684375017946156764233727E-24,
    -0.1881448598976133459864609148108E-24,
    +0.7744916111507730845444328478037E-25,
    -0.3208512760585368926702703826261E-25,
    +0.1337445542910839760619930421384E-25,
    -0.5608671881802217048894771735210E-26,
    +0.2365839716528537483710069473279E-26,
    -0.1003656195025305334065834526856E-26,
    +0.4281490878094161131286642556927E-27,
    -0.1836345261815318199691326958250E-27,
    +0.7917798231349540000097468678144E-28,
    -0.3431542358742220361025015775231E-28,
    +0.1494705493897103237475066008917E-28,
    -0.6542620279865705439739042420053E-29,
    +0.2877581395199171114340487353685E-29,
    -0.1271557211796024711027981200042E-29,
    +0.5644615555648722522388044622506E-30,
    -0.2516994994284095106080616830293E-30,
    +0.1127259818927510206370368804181E-30,
    -0.5069814875800460855562584719360E-31 };
  static double e11cs[29] = {
    -0.16113461655571494025720663927566180E+02,
    +0.77940727787426802769272245891741497E+01,
    -0.19554058188631419507127283812814491E+01,
    +0.37337293866277945611517190865690209,
    -0.56925031910929019385263892220051166E-01,
    +0.72110777696600918537847724812635813E-02,
    -0.78104901449841593997715184089064148E-03,
    +0.73880933562621681878974881366177858E-04,
    -0.62028618758082045134358133607909712E-05,
    +0.46816002303176735524405823868362657E-06,
    -0.32092888533298649524072553027228719E-07,
    +0.20151997487404533394826262213019548E-08,
    -0.11673686816697793105356271695015419E-09,
    +0.62762706672039943397788748379615573E-11,
    -0.31481541672275441045246781802393600E-12,
    +0.14799041744493474210894472251733333E-13,
    -0.65457091583979673774263401588053333E-15,
    +0.27336872223137291142508012748799999E-16,
    -0.10813524349754406876721727624533333E-17,
    +0.40628328040434303295300348586666666E-19,
    -0.14535539358960455858914372266666666E-20,
    +0.49632746181648636830198442666666666E-22,
    -0.16208612696636044604866560000000000E-23,
    +0.50721448038607422226431999999999999E-25,
    -0.15235811133372207813973333333333333E-26,
    +0.44001511256103618696533333333333333E-28,
    -0.12236141945416231594666666666666666E-29,
    +0.32809216661066001066666666666666666E-31,
    -0.84933452268306432000000000000000000E-33 };
  static double e12cs[25] = {
    -0.3739021479220279511668698204827E-01,
    +0.4272398606220957726049179176528E-01,
    -0.130318207984970054415392055219726,
    +0.144191240246988907341095893982137E-01,
    -0.134617078051068022116121527983553E-02,
    +0.107310292530637799976115850970073E-03,
    -0.742999951611943649610283062223163E-05,
    +0.453773256907537139386383211511827E-06,
    -0.247641721139060131846547423802912E-07,
    +0.122076581374590953700228167846102E-08,
    -0.548514148064092393821357398028261E-10,
    +0.226362142130078799293688162377002E-11,
    -0.863589727169800979404172916282240E-13,
    +0.306291553669332997581032894881279E-14,
    -0.101485718855944147557128906734933E-15,
    +0.315482174034069877546855328426666E-17,
    -0.923604240769240954484015923200000E-19,
    +0.255504267970814002440435029333333E-20,
    -0.669912805684566847217882453333333E-22,
    +0.166925405435387319431987199999999E-23,
    -0.396254925184379641856000000000000E-25,
    +0.898135896598511332010666666666666E-27,
    -0.194763366993016433322666666666666E-28,
    +0.404836019024630033066666666666666E-30,
    -0.807981567699845120000000000000000E-32 };
  double eta, value;
  static double xmax = 0.0;
  static int ntae10, ntae11, ntae12, ntae13, ntae14, nte11, nte12;
  ntae10 = ntae11 = ntae12 = ntae13 = ntae14 = nte11 = nte12 = 0;
  if (ntae10 == 0) {
    eta = 0.1*r8_mach(3);
    ntae10 = r8_inits(ae10cs, 50, eta);
    ntae11 = r8_inits(ae11cs, 60, eta);
    ntae12 = r8_inits(ae12cs, 41, eta);
    nte11  = r8_inits(e11cs, 29, eta);
    nte12  = r8_inits(e12cs, 25, eta);
    ntae13 = r8_inits(ae13cs, 50, eta);
    ntae14 = r8_inits(ae14cs, 64, eta);
    xmax = -sun_log(r8_mach(1));
    xmax = xmax - sun_log(xmax);
  }
  if (x <= -32.0) {
    value = sun_exp(- x)/x*(1.0 + r8_csevl(64.0/x + 1.0, ae10cs, ntae10));
  } else if (x <= -8.0) {
    value = sun_exp(- x)/x*(1.0 + r8_csevl((64.0/x + 5.0)/3.0, ae11cs, ntae11));
  } else if (x <= -4.0) {
    value = sun_exp(-x)/x*(1.0 + r8_csevl(16.0/x + 3.0, ae12cs, ntae12));
  } else if (x <= -1.0) {
    value = - sun_log(-x) + r8_csevl((2.0*x + 5.0)/3.0, e11cs, nte11);
  } else if (x == 0.0) {
    return AGN_NAN;
  } else if (x <= 1.0) {
    value = (-sun_log(fabs(x)) - 0.6875 + x) + r8_csevl(x, e12cs, nte12);
  } else if (x <= 4.0) {
    value = sun_exp(-x)/x*(1.0 + r8_csevl((8.0/x - 5.0)/3.0, ae13cs, ntae13));
  } else if (x <= xmax) {
    value = sun_exp(-x)/x*(1.0 + r8_csevl(8.0/x - 1.0, ae14cs, ntae14));
  } else {
    value = 0.0;
  }
  return value;
}


/******************************************************************************/
/*
  Purpose:
    digamma() calculates DIGAMMA ( X ) = d ( LOG ( GAMMA ( X ) ) ) / dX

  Licensing:
    This code is distributed under the MIT license.

  Modified:
    20 March 2016

  Author:
    Original FORTRAN77 version by Jose Bernardo.
    This version by John Burkardt.

  Reference:
    Jose Bernardo,
    Algorithm AS 103:
    Psi ( Digamma ) Function,
    Applied Statistics,
    Volume 25, Number 3, 1976, pages 315-317.

  Input:
    double X, the argument of the digamma function.
    0 < X.

  Output:
    int *IFAULT, error flag.
    0, no error.
    1, X <= 0.

    double DIGAMMA, the value of the digamma function at X.

  Note:
    Cephes' psi() is more accuarate than this r8_digamma() implementation. */

double r8_digamma (double x) {
  static long double c = 8.5L;
  long double r, value, x2;
  /* Check the input. */
  if (x <= 0.0L) {
    value = AGN_NAN;
  } else {
    /* initialize */
    /* Use approximation for small argument. */
    if (x <= 0.000001L) {
      return -EULERGAMMAld - 1.0L/x + ZETA2ld*x;  /* Zeta(2) = Pi^2/6 = 1.64493406.. in Maple */
    }
    /* reduce to DIGAMA(X + N). */
    value = 0.0L;
    x2 = x;
    while (x2 < c) {
      value -= 1.0L/x2;
      x2 += 1.0L;
    }
    /* Use Stirling's (actually de Moivre's) expansion. */
    r = 1.0L/x2;
    /* value = value + tools_logl(x2) - 0.5*r; */
    value += fmal(-0.5L, r, tools_logl(x2));
    r *= r;
    value = value
      - r*(1.0L/12.0L
      - r*(1.0L/120.0L
      - r*(1.0L/252.0L
      - r*(1.0L/240.0L
      - r*(1.0L/132.0L
      - r*(691.0L/32760.0L))))));
  }
  return value;
}


/******************************************************************************/
/*
   Purpose:
     RC computes the elementary integral RC(x, y).

   Source:
     C code taken from file toms577.c.

   Discussion:
     This function computes the elementary integral

                       infinity
                      /
                     |                     1
     RC(x, y) = 1/2  |          ----------------------- dt
                     |          sqrt((t + x)*(t + y)^2)
                    /
                      0

     where X is nonnegative and Y is positive.  The duplication
     theorem is iterated until the variables are nearly equal,
     and the function is then expanded in Taylor series to fifth
     order.

     Logarithmic, inverse circular, and inverse hyperbolic
     functions can be expressed in terms of RC.

     Check by addition theorem:

       RC(X,X+Z) + RC(Y,Y+Z) = RC(0,Z),
       where X, Y, and Z are positive and X * Y = Z * Z.

   Licensing:
     This code is distributed under the MIT license.

   Modified:
     02 June 2018

   Author:
     Original FORTRAN77 version by Bille Carlson, Elaine Notis.
     This C version by John Burkardt.

   Reference:
     Bille Carlson,
     Computing Elliptic Integrals by Duplication,
     Numerische Mathematik,
     Volume 33, 1979, pages 1-16.

     Bille Carlson, Elaine Notis,
     Algorithm 577, Algorithms for Incomplete Elliptic Integrals,
     ACM Transactions on Mathematical Software,
     Volume 7, Number 3, pages 398-403, September 1981.

   Parameters:
     Input, double X, Y, the arguments in the integral.
     Input, double ERRTOL, the error tolerance.
     Relative error due to truncation is less than
       16 * ERRTOL ^ 6 / (1 - 2 * ERRTOL).
     Sample choices:
       ERRTOL   Relative truncation error less than
       1.D-3    2.D-17
       3.D-3    2.D-14
       1.D-2    2.D-11
       3.D-2    2.D-8
       1.D-1    2.D-5
     Output, int *IERR, the error flag.
     0, no error occurred.
     1, abnormal termination.

   Note: Maple V Release 4 formula:

     rc := proc(x, y)
        1/2 * evalf(Int(1/sqrt((t + x)*(t + y)^2), t = 0 .. infinity));
     end;

    which is equivalent to:

    RC_manual := proc(x, y)
       if y > 0 and y < x then
          1/sqrt(y) * ln((sqrt(x) + sqrt(x-y)) / (sqrt(x) - sqrt(x-y)));
       elif x >= 0 and x < y then
          arccos(sqrt(x/y)) / sqrt(y-x);
       elif x = y and x > 0 then # Add this case
          1/sqrt(x);
       else
          undefined;
          #ERROR("Cases for RC(x,y) not covered or invalid inputs (x,y must be positive).");
       fi;
    end;
*/
double r8_rc (double x, double y, double errtol) {
  double c1, c2, lambda, value, xn, yn, mu, s, sn;
  const double lolim = 3.0E-78;
  const double uplim = 1.0E+75;
  /* LOLIM AND UPLIM DETERMINE THE RANGE OF VALID ARGUMENTS.
     LOLIM IS NOT LESS THAN THE MACHINE MINIMUM MULTIPLIED BY 5.
     UPLIM IS NOT GREATER THAN THE MACHINE MAXIMUM DIVIDED BY 5. */
  if (x < 0.0 || y <= 0.0 || (x + y) < lolim || uplim < x || uplim < y) {
    return AGN_NAN;
  }
  xn = x;
  yn = y;
  while (1) {
    mu = (xn + yn + yn)/3.0;
    sn = (yn + mu)/mu - 2.0;
    if (fabs(sn) < errtol) {
      c1 = 1.0/7.0;
      c2 = 9.0/22.0;
      /* s = sn*sn*(0.3 + sn*(c1 + sn*(0.375 + sn*c2))); */
      s = sn*sn*fma(sn, fma(sn, fma(sn, c2, 0.375), c1), 0.3);  /* 6.4.10 improvement */
      value = (1.0 + s)/sqrt(mu);
      return value;
    }
    /* lambda = 2.0*sqrt(xn)*sqrt(yn) + yn; */
    lambda = fma(2.0*sqrt(xn), sqrt(yn), yn);  /* 6.4.10 improvement */
    xn = 0.25*(xn + lambda);
    yn = 0.25*(yn + lambda);
  }
}