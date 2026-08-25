/* The cordic Agena binding, initiated March 11, 2014

   This binding is based on C procedures written by John Burkardt, taken from:
   http://people.sc.fsu.edu/~jburkardt/c_src/cordic/cordic.c

   This code is distributed under the MIT Licence*), same as John Burkardt's.

   The following changes to Mr. Burkardts original C code have been made, in chronological order:

   - All lookup tables have been `staticalized` to reduce computation times.
   - arctan_cordic has been patched to return the same values with negative arguments as C's atan2 function.
   - arctan_cordic has been extended to also return the hypotenuse.
   - The package now also provides the CORDIC Gain constant 0.60725293500888125..., computed with Maple V Release 4.
   - Various precompiled lookup tables were identical and have been reduced to respectively one for trigonometric
     functions and one for logarithm/exponent functions.
   - The lookup table for exponential functions has been recomputed using Maple V Release 4 with an accuracy of
     50 digits. It also has been extended from 25 to 52 entries.
   - The lookup tables of the expansion factors *kprod and of the precomputed angles have been recompiled using
     Maple V Release 4 with an accuracy of 50 digits. They both now also contain 64 entries each.
   - Despite the expansion of the lookup tables, the defaults for the number of iterations has not been changed.
     They might be altered by redefining the *_ITERS #defines.
   - Local definitions of Pi have been replaced by the C #define imported from agnhlps.h.
   - Local definitions of e = exp(1) have been replaced by a C #define imported from agnhlps.h.
   - free's have been replaced by the xfree macro, thus nulling all pointers after deallocation.
   - The C code has been reformatted to the Lua-style and shortened.
   - Some few function parameter names have been changed.
   - Introduced SURD_LENGTH_ITERS setting for sqrt, crbt.
   - Added checks for +/-infinity and undefined to prevent ln_cordic and exp_cordic from going into an infinite loop.
   - Added malloc protection for ln_cordic and exp_cordic.

   The following multiple references have been abridged and are given behind the name of the respective author:

     [1] Jean-Michel Muller, `Elementary Functions: Algorithms and Implementation`,
         Second Edition, Birkhaeuser, 2006, ISBN13: 978-0-8176-4372-0, LC: QA331.M866.

     [2] Frederick Ruckdeschel, `BASIC Scientific Subroutines`, Volume II,
         McGraw-Hill, 1980, ISBN: 0-07-054202-3, LC: QA76.95.R82.

   - The original C functions `timestamp`, `arccos_values`, `arcsin_values`, `arctan_values`,
     `ln_values`, `cbrt_values`, `sin_values`, `cos_values`, `tan_values`, `sqrt_values`, `exp_values`,
     `r8_uniform_01`, `i4_min`, and `i4_huge` have been removed.

   A note: the CORDIC algorithm (CORDIC = COordinate Rotation DIgital Computer) also known as the `Volder's algorithm`,
   is used to calculate hyperbolic, trigonometric, and root functions, on hardware not featuring multipliers, requiring
   only addition, subtraction, bitshift and table lookup.

   The algorithm has been developed in 1959 by Kack E. Volder to improve an aviation system. According to Wikipedia, it
   has not only been used in pocket calculators, but also in x87 FPUs, in CPUs prior to Intel 80486 and in Motorola's
   68881, in signal and image processing, communication systems, robotics, and also 3D graphics - and many other
   applications.

   This binding to John Burkardt's CORDIC implementation, however, uses multiplication, divisions, and the absolute
   function, and has been chosen to implement hardware-independent versions of sqrt, cbrt, ln, exp, hypot, sin, sinh,
   cos, cosh, tan, tanh, arcsin, arccos, arctan, and arctanh.

   The Maple V statements to recompute the lookup tables are given below.

   The Agena binding has been written by Alexander Walz.

   *) John Burkardt originally published the code under the GNU LGPL licence. At least as of April 20, 2025, his cordic.c
      source file available on his website professes the MIT licence. */

#define AGENA_LIBVERSION	"cordic v0.2.2 as of April 20, 2025\n"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define cordic_c
#define LUA_LIB

#include "agena.h"
#include "agenalib.h"
#include "agncmpt.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "cordic.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(__ANSI__))
#define AGENA_CORDICLIBNAME "cordic"
LUALIB_API int (luaopen_cordic) (lua_State *L);
#endif


/* Some tables */

#define ANGLES_LENGTH        64
#define EXPLNA_LENGTH        52
#define KPROD_LENGTH         64

#define ANGLES_LENGTH_ITERS  60
#define EXPLNA_LENGTH_ITERS  25
#define KPROD_LENGTH_ITERS   33
#define SURD_LENGTH_ITERS    53

/* Angles lookup table for trigonometric functions

Maple formula:

interface(prettyprint=0);
for i in seq(evalf(arctan(2^(-i))), i=0 .. 63) do print(i) od;

*/

static double TRIG_CORDIC_angles[ANGLES_LENGTH] = {
  0.78539816339744830961566084581987572104929234984378,
  0.46364760900080611621425623146121440202853705428612,
  0.24497866312686415417208248121127581091414409838118,
  0.12435499454676143503135484916387102557317019176980,
  0.62418809995957348473979112985505113606273887797499e-1,
  0.31239833430268276253711744892490977032495663725400e-1,
  0.15623728620476830802801521256570318911114139800905e-1,
  0.78123410601011112964633918421992816212228117250147e-2,
  0.39062301319669718276286653114243871403574901152029e-2,
  0.19531225164788186851214826250767139316107467772335e-2,
  0.97656218955931943040343019971729085163419701581009e-3,
  0.48828121119489827546923962564484866619236113313500e-3,
  0.24414062014936176401672294325965998621241779097062e-3,
  0.12207031189367020423905864611795630093082940901579e-3,
  0.61035156174208775021662569173829153785143536833346e-4,
  0.30517578115526096861825953438536019750949675119438e-4,
  0.15258789061315762107231935812697885137429238144576e-4,
  0.76293945311019702633884823401050905863507439184681e-5,
  0.38146972656064962829230756163729937228052573039689e-5,
  0.19073486328101870353653693059172441687143421654502e-5,
  0.95367431640596087942067068992311239001963412449879e-6,
  0.47683715820308885992758382144924707587049404378664e-6,
  0.23841857910155798249094797721893269783096898769063e-6,
  0.11920928955078068531136849713792211264596758766459e-6,
  0.59604644775390554413921062141788874250030195782366e-7,
  0.29802322387695303676740132767709503349043907067445e-7,
  0.14901161193847655147092516595963247108248930025965e-7,
  0.74505805969238279871365645744953921132066925545666e-8,
  0.37252902984619140452670705718119235836719483287370e-8,
  0.18626451492309570290958838214764904345065282835739e-8,
  0.93132257461547851535573547768456130389292649614929e-9,
  0.46566128730773925777884193471057016297347863891562e-9,
  0.23283064365386962890204274183882127037127429320498e-9,
  0.11641532182693481445259909272985265879639645738001e-9,
  0.58207660913467407226496761591231582349549156257795e-10,
  0.29103830456733703613273032698903947793693632003640e-10,
  0.14551915228366851806639597837362993474211703608937e-10,
  0.72759576141834259033201841046703741842764629388821e-11,
  0.36379788070917129516601402005837967730345578669779e-11,
  0.18189894035458564758300761188229745966293197333603e-11,
  0.90949470177292823791503881172787182457866496666966e-12,
  0.45474735088646411895751949990348397807233312083370e-12,
  0.22737367544323205947875976170668549725904164010421e-12,
  0.11368683772161602973937988232271068715738020501303e-12,
  0.56843418860808014869689941345026335894672525626628e-13,
  0.28421709430404007434844970695472041986834065703329e-13,
  0.14210854715202003717422485350605880248354258212916e-13,
  0.71054273576010018587112426756616725310442822766145e-14,
  0.35527136788005009293556213378756778163805352845768e-14,
  0.17763568394002504646778106689434441020475669105721e-14,
  0.88817841970012523233890533447242270025594586382151e-15,
  0.44408920985006261616945266723629893128199323297769e-15,
  0.22204460492503130808472633361816041328524915412221e-15,
  0.11102230246251565404236316680908157509815614426528e-15,
  0.55511151231257827021181583404540958606019518033160e-16,
  0.27755575615628913510590791702270500685127439754145e-16,
  0.13877787807814456755295395851135253015328429969268e-16,
  0.69388939039072283776476979255676268417598037461585e-17,
  0.34694469519536141888238489627838134626418504682698e-17,
  0.17347234759768070944119244813919067365411688085337e-17,
  0.86736173798840354720596224069595336892311485106672e-18,
  0.43368086899420177360298112034797668454312373138334e-18,
  0.21684043449710088680149056017398834228175765392292e-18,
  0.10842021724855044340074528008699417114215330049036e-18 };

/* Cordic Gain

In Maple V Release 4:

Digits := 50:
evalf(product(cos(arctan(2.0^(-i))), i=0 .. 63));

*/

static double
  GAIN = 0.60725293500888125616944675250492826311358055614085;


/* In Maple V Release 4:

Digits := 50:
seq(exp(2.^(-i)), i=1 .. 60);

*/

static double EXPLN_CORDIC_a[EXPLNA_LENGTH] = {
  1.6487212707001281468486507878141635716537761007101,
  1.2840254166877414840734205680624364583362808652815,
  1.1331484530668263168290072278117938725655031317452,
  1.0644944589178594295633905946428896731007254436494,
  1.0317434074991026709387478152815071441944983266418,
  1.0157477085866857474585350720823517489067162884822,
  1.0078430972064479776934535597601235791933921498840,
  1.0039138893383475734436096039034602818988135564304,
  1.0019550335910028120465188980474772158673556263217,
  1.0009770394924165352428452926116065064658516291817,
  1.0004884004786944731261736238071633537881054969273,
  1.0002441704297478549370052339241357737575196231198,
  1.0001220777633837710765035196704053169651875701326,
  1.0000610370189330454217791205985471093033817019158,
  1.0000305180437910242954512848125412431703292189215,
  1.0000152589054784139481400426224806617301870123486,
  1.0000076294236351544717431843303382200613086237420,
  1.0000038147045415918660507877128903323298618163808,
  1.0000019073504518030600287252476535393929977574166,
  1.0000009536747711537454467882495568742836518855328,
  1.0000004768372718899807916543919475044056566776088,
  1.0000002384186075232741891586680785801190380695148,
  1.0000001192092966562088899453260246612120070545342,
  1.0000000596046465517474996932904595142678801032203,
  1.0000000298023228317845267616925826548048466750592,
  1.0000000149011613048699592639693977987068968587725,
  1.0000000074505806246794038095606313517384277586942,
  1.0000000037252903054008079750236930997664443130705,
  1.0000000018626451509656805083038651841752235229184,
  1.0000000009313225750491593847538340347920469844993,
  1.0000000004656612874161594750778794760474193558540,
  1.0000000002328306436809746832204912399308981321644,
  1.0000000001164153218337110780314223563476279035653,
  1.0000000000582076609151614731211039698825213321365,
  1.0000000000291038304571572200869125088200822172479,
  1.0000000000145519152284727309250479261237020454439,
  1.0000000000072759576142098956829220735832606981028,
  1.0000000000036379788070983303965605884961070728903,
  1.0000000000018189894035475108370551851834382555195,
  1.0000000000009094947017733418282213157017234997921,
  1.0000000000004547473508865675165340886753826169371,
  1.0000000000002273736754432579088729020496989969829,
  1.0000000000001136868377216224920879154535861045893,
  1.0000000000000568434188608096304568238340690393153,
  1.0000000000000284217094304044113316284438649958498,
  1.0000000000000142108547152021046916183536415519026,
  1.0000000000000071054273576010271022602097482188119,
  1.0000000000000035527136788005072402428631059925419,
  1.0000000000000017763568394002520423996211109698575,
  1.0000000000000008881784197001256267693579449786757,
  1.0000000000000004440892098500627147770658198628184,
  1.0000000000000002220446049250313327366296217747848 };


/* Expansion Factors

   Maple formula:

Digits := 50:
seq(product(1/(sqrt(1.+2^(-2*i))), i=0 .. n), n=0 .. 31);

*/

static double COSSIN_CORDIC_kprod[KPROD_LENGTH] = {
  0.70710678118654752440084436210484903928483593768850,
  0.63245553203367586639977870888654370674391102786505,
  0.61357199107789634960780908775804088626146764977978,
  0.60883391251775242102211350754738991314362452861743,
  0.60764825625616820092932166030952304596770311358196,
  0.60735177014129595905351239038776428178660638348265,
  0.60727764409352599904691536733758899800545664222800,
  0.60725911229889273006029454182250359069013905201157,
  0.60725447933256232971739808632515662631099742021904,
  0.60725332108987516334343519856376641855657907350366,
  0.60725303152913433540228465466152814147997674906079,
  0.60725295913894481363035179763757115628137291754365,
  0.60725294104139716351297018642410355165360171770258,
  0.60725293651701023412897124207973889082336069267065,
  0.60725293538591350072955560274527678638115476283838,
  0.60725293510313931731386319806954337503038578376420,
  0.60725293503244577145582519095908554951209634426566,
  0.60725293501477238499105850755990874945110429275642,
  0.60725293501035403837485076285876351132859535226308,
  0.60725293500924945172079782206776770892208973515024,
  0.60725293500897330505728452408153691418768695424575,
  0.60725293500890426839140619566069910023284970457798,
  0.60725293500888700922493661331022213953948595077559,
  0.60725293500888269443331921770727368016585095103947,
  0.60725293500888161573541486880557848912242377292040,
  0.60725293500888134606093878158009481159906582585787,
  0.60725293500888127864231975977372014973307001704689,
  0.60725293500888126178766500432212625036124879471616,
  0.60725293500888125757400131545922776089921084725045,
  0.60725293500888125652058539324350313762000869526633,
  0.60725293500888125625723141268957198174310236570045,
  0.60725293500888125619139291755108919277030667133585,
  0.60725293500888125617493329376646849552688467824639,
  0.60725293500888125617081838782031332121601523813038,
  0.60725293500888125616978966133377452763829700673617,
  0.60725293500888125616953247971213982924386739442731,
  0.60725293500888125616946818430673115464525998794631,
  0.60725293500888125616945211045537898599560813611334,
  0.60725293500888125616944809199254094383319517314176,
  0.60725293500888125616944708737683143329259193239805,
  0.60725293500888125616944683622290405565744112221204,
  0.60725293500888125616944677343442221124865341966553,
  0.60725293500888125616944675773730175014645649402892,
  0.60725293500888125616944675381302163487090726261975,
  0.60725293500888125616944675283195160605201995476747,
  0.60725293500888125616944675258668409884729812780440,
  0.60725293500888125616944675252536722204611767106363,
  0.60725293500888125616944675251003800284582255687842,
  0.60725293500888125616944675250620569804574877833212,
  0.60725293500888125616944675250524762184573033369556,
  0.60725293500888125616944675250500810279572572253642,
  0.60725293500888125616944675250494822303322456974662,
  0.60725293500888125616944675250493325309259928154919,
  0.60725293500888125616944675250492951060744295949979,
  0.60725293500888125616944675250492857498615387898744,
  0.60725293500888125616944675250492834108083160885934,
  0.60725293500888125616944675250492828260450104132731,
  0.60725293500888125616944675250492826798541839944427,
  0.60725293500888125616944675250492826433064773897353,
  0.60725293500888125616944675250492826341695507385580,
  0.60725293500888125616944675250492826318853190757634,
  0.60725293500888125616944675250492826313142611600647,
  0.60725293500888125616944675250492826311714966811397,
  0.60725293500888125616944675250492826311358055614086 };

/*
  Purpose: ANGLE_SHIFT shifts angle ALPHA to lie between BETA and BETA+2PI.

  Discussion:

    The input angle ALPHA is shifted by multiples of 2 * PI to lie
    between BETA and BETA+2*PI.

    The resulting angle GAMMA has all the same trigonometric function
    values as ALPHA.

  Modified: 19 January 2012

  Author: John Burkardt

  Parameters:

    Input, double ALPHA, the angle to be shifted.
    Input, double BETA, defines the lower endpoint of the angle range.

    Output, double ANGLE_SHIFT, the shifted angle.
*/
double angle_shift (double alpha, double beta) {
  return (alpha < beta) ? beta - fmod(beta - alpha, 2*PI) + 2*PI :
                          beta + fmod(alpha - beta, 2*PI);
}


/*
  Purpose: ARCCOS_CORDIC returns the arccosine of an angle using the CORDIC method.

  Modified: 19 January 2012

  Author: John Burkardt [1]

  Parameters:

    Input, double T, the cosine of an angle.  -1 <= T <= 1.
    Input, int N, the number of iterations to take. (A value of 10 is low. Good accuracy is achieved
      with 20 or more iterations.)

    Output, double ARCCOS_CORDIC, an angle whose cosine is T.

  Local Parameters:

    Local, double ANGLES(60) = arctan((1/2)^(0:59));
*/

double arccos_cordic (double x, int n) {
  int i, j;
  double angle, poweroftwo, sigma, sign_z2, theta, x1, x2, y1, y2;
  if (1 < fabs(x)) return AGN_NAN;
  theta = 0;
  x1 = 1;
  y1 = 0;
  poweroftwo = 1;
  angle = AGN_NAN;
  for (j=1; j <= n; j++) {
    sign_z2 = (y1 < 0) ? -1 : 1;
    sigma = (x <= x1) ? sign_z2 : -sign_z2;
    angle = (j <= ANGLES_LENGTH_ITERS) ? TRIG_CORDIC_angles[j-1] : angle/2;
    for (i=1; i <= 2; i++) {
      x2 =                      x1 - sigma * poweroftwo * y1;
      y2 = sigma * poweroftwo * x1 +                      y1;
      x1 = x2;
      y1 = y2;
    }
    theta += 2*sigma*angle;
    x += x*poweroftwo*poweroftwo;
    poweroftwo /= 2;
  }
  return theta;
}

static int cordic_carccos (lua_State *L) {
  int n = agnL_optposint(L, 2, ANGLES_LENGTH_ITERS);
  lua_pushnumber(L, arccos_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/*
  Purpose: ARCSIN_CORDIC returns the arcsine of an angle using the CORDIC method.

  Modified: 19 January 2012

  Author: John Burkardt [1]

  Parameters:

    Input, double T, the sine of an angle.  -1 <= T <= 1.
    Input, int N, the number of iterations to take. (A value of 10 is low. Good accuracy is achieved
      with 20 or more iterations.)

    Output, double ARCSIN_CORDIC, an angle whose sine is T.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59));
*/

double arcsin_cordic (double x, int n) {
  int i, j;
  double angle, poweroftwo, sigma, sign_z1, theta, x1, x2, y1, y2;
  if (1 < fabs(x)) return AGN_NAN;
  theta = 0;
  x1 = 1;
  y1 = 0;
  angle = AGN_NAN;
  poweroftwo = 1;
  for (j=1; j <= n; j++) {
    sign_z1 = (x1 < 0) ? -1 : 1;
    sigma = (y1 <= x) ? sign_z1 : -sign_z1;
    angle = (j <= ANGLES_LENGTH_ITERS) ? TRIG_CORDIC_angles[j-1] : angle / 2;
    for (i=1; i <= 2; i++) {
      x2 =                      x1 - sigma * poweroftwo * y1;
      y2 = sigma * poweroftwo * x1 +                      y1;
      x1 = x2;
      y1 = y2;
    }
    theta += 2 * sigma * angle;
    x += x * poweroftwo * poweroftwo;
    poweroftwo = poweroftwo / 2;
  }
  return theta;
}

static int cordic_carcsin (lua_State *L) {
  int n = agnL_optposint(L, 2, ANGLES_LENGTH_ITERS);
  lua_pushnumber(L, arcsin_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/*
  Purpose: ARCTAN_CORDIC returns the arctangent of an angle using the CORDIC method.

  Modified: 15 June 2007

  Author: John Burkardt [1]

  Further reference for the Agena extensions:
  http://forums.parallax.com/showthread.php/127241-CORDIC-for-dummies

  Parameters:

    Input, double X, Y, define the tangent of an angle as Y/X.

    Input, int N, the number of iterations to take. (A value of 10 is low. Good accuracy is
      achieved with 20 or more iterations.)

    Output, double ARCTAN_CORDIC, the angle whose tangent is Y/X.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59));
*/

double arctan_cordic (double x, double y, int n, double *hypotenuse) {
  int j;
  double angle, poweroftwo, sigma, sign_factor, theta, x1, x2, y1, y2, offset;
  x1 = x;
  y1 = y;
  offset = 0;
  angle = AGN_NAN;
  /* account for signs */
  if (x1 < 0 && y1 < 0) {
    x1 = -x1;
    y1 = -y1;
  }
  if (x1 < 0) {
    x1 = -x1;
    sign_factor = -1;
  } else if (y1 < 0) {
    y1 = -y1;
    sign_factor = -1;
  } else
    sign_factor = +1;
  theta = 0;
  poweroftwo = 1;
  for (j=1; j <= n; j++) {
    sigma = (y1 <= 0) ? 1 : -1;
    angle = (j <= ANGLES_LENGTH_ITERS) ? TRIG_CORDIC_angles[j-1] : angle / 2;
    x2 =                      x1 - sigma * poweroftwo * y1;
    y2 = sigma * poweroftwo * x1 +                      y1;
    theta -= sigma * angle;
    x1 = x2;
    y1 = y2;
    poweroftwo /= 2;
  }
  if (x < 0) {   /* added for Agena 2.1.6 */
    offset = (y >= 0) ? PI : -PI;
  }
  *hypotenuse = x1 * GAIN;  /* added Agena 2.1.6 */
  return theta * sign_factor + offset;
}

static int cordic_carctan2 (lua_State *L) {
  lua_Number hypotenuse;
  int n = agnL_optposint(L, 3, ANGLES_LENGTH_ITERS);
  lua_pushnumber(L, arctan_cordic(agn_checknumber(L, 2), agn_checknumber(L, 1), n, &hypotenuse));
  return 1;
}

static int cordic_chypot (lua_State *L) {
  lua_Number hypotenuse;
  int n = agnL_optposint(L, 3, ANGLES_LENGTH_ITERS);
  arctan_cordic(agn_checknumber(L, 2), agn_checknumber(L, 1), n, &hypotenuse);
  lua_pushnumber(L, hypotenuse);
  return 1;
}


/*
  Purpose: SQRT_CORDIC returns the square root of a value using the CORDIC method.

  Modified: 19 January 2012

  Author: John Burkardt

  Parameters:

    Input, double X, the number whose square root is desired.
    Input, int N, the number of iterations to take. (This is essentially the number of
      binary digits of accuracy, and might go as high as 53.)

    Output, double SQRT_CORDIC, the approximate square root of X.
*/

double sqrt_cordic (double x, int n) {
  int i;
  double poweroftwo, y;
  if (x < 0) return AGN_NAN;
  if (x == 0 || x == 1) return x;
  y = HUGE_VAL;
  poweroftwo = 1;
  if (x < 1) {
    while ( x <= poweroftwo * poweroftwo) poweroftwo /= 2;
    y = poweroftwo;
  } else if (1 < x) {
    while ( poweroftwo * poweroftwo <= x) poweroftwo *= 2;
    y = poweroftwo / 2;
  }
  for (i=1; i <= n; i++) {
    poweroftwo /= 2;
    if ((y + poweroftwo) * (y + poweroftwo) <= x)
      y += poweroftwo;
  }
  return y;
}

static int cordic_csqrt (lua_State *L) {
  int n = agnL_optposint(L, 2, SURD_LENGTH_ITERS);
  lua_pushnumber(L, sqrt_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/*
  Purpose: CBRT_CORDIC returns the cube root of a value using the CORDIC method.

  Modified: 19 January 2012

  Author: John Burkardt

  Parameters:

    Input, double X, the number whose cube root is desired.
    Input, int N, the number of iterations to take. (This is essentially the number of
      binary digits of accuracy, and might go as high as 53.)

    Output, double CBRT_CORDIC, the approximate cube root of X.
*/

double cbrt_cordic (double x, int n) {
  int i;
  double poweroftwo, x_mag, y;
  y = HUGE_VAL;
  x_mag = fabs(x);
  if (x_mag == 0) return 0;
  if (x_mag == 1) return x;
  poweroftwo = 1;
  if (x_mag < 1) {
    while (x_mag <= poweroftwo * poweroftwo * poweroftwo) poweroftwo /= 2;
    y = poweroftwo;
  } else if (1 < x_mag) {
    while (poweroftwo * poweroftwo * poweroftwo <= x_mag) poweroftwo *= 2;
    y = poweroftwo / 2;
  }
  for (i=1; i <= n; i++) {
    poweroftwo /= 2;
    if ((y + poweroftwo) * (y + poweroftwo)* (y + poweroftwo) <= x_mag) {
      y += poweroftwo;
    }
  }
  return (x < 0) ? -y : y;
}

static int cordic_ccbrt (lua_State *L) {
  int n = agnL_optposint(L, 2, SURD_LENGTH_ITERS);
  lua_pushnumber(L, cbrt_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/*
  Purpose: COSSIN_CORDIC returns the sine and cosine of an angle by the CORDIC method.

  Modified: 19 January 2012

  Author:

    Based on MATLAB code in a Wikipedia article.
    C++ version by John Burkardt

  Parameters:

    Input, double BETA, the angle (in radians).
    Input, int N, the number of iterations to take. (A value of 10 is low. Good accuracy is
      achieved with 20 or more iterations.)

    Output, double *C, *S, the cosine and sine of the angle.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59));

    Local, double KPROD(33).  KPROD(j) = product ( 0 <= i <= j) K(i)
    where K(i) = 1 / sqrt ( 1 + (1/2)^(2i)).
*/

void cossin_cordic (double x, int n, double *c, double *s) {
  int j;
  double angle, c2, factor, poweroftwo, s2, sigma, sign_factor, theta;
  /* shift angle to interval [-PI,PI] */
  theta = angle_shift(x, -PI);
  /* shift angle to interval [-PI/2,PI/2] and account for signs */
  if (theta < - 0.5 * PI) {
    theta = theta + PI;
    sign_factor = -1;
  } else if (0.5 * PI < theta) {
    theta = theta - PI;
    sign_factor = -1;
  } else
    sign_factor = +1;
  /* initialize loop variables: */
  *c = 1;
  *s = 0;
  poweroftwo = 1;
  angle = TRIG_CORDIC_angles[0];
  /* iterations */
  for (j=1; j <= n; j++) {
    sigma = (theta < 0) ? -1 : 1;
    factor = sigma * poweroftwo;
    c2 =          *c - factor * *s;
    s2 = factor * *c +          *s;
    *c = c2;
    *s = s2;
    /* Update the remaining angle */
    theta -= sigma * angle;
    poweroftwo /= 2;
    /* Update the angle from table, or eventually by just dividing by two */
    angle = (ANGLES_LENGTH_ITERS < j + 1) ? angle / 2 : TRIG_CORDIC_angles[j];
  }
  /* adjust length of output vector to be [cos(x), sin(x)]
     KPROD is essentially constant after a certain point, so if N is large, just take the last available value */
  if (0 < n) {
    *c = *c * COSSIN_CORDIC_kprod[fMin(n, KPROD_LENGTH_ITERS) - 1];
    *s = *s * COSSIN_CORDIC_kprod[fMin(n, KPROD_LENGTH_ITERS) - 1];
  }
  /* adjust for possible sign change because angle was originally not in quadrant 1 or 4 */
  *c = sign_factor * *c;
  *s = sign_factor * *s;
  return;
}

static int cordic_csin (lua_State *L) {
  lua_Number c, s;
  int n = agnL_optposint(L, 2, ANGLES_LENGTH_ITERS);  /* n = 27 is great, with just two deviations from Sun's sin implementation */
  cossin_cordic(agn_checknumber(L, 1), n, &c, &s);
  lua_pushnumber(L, s);
  return 1;
}

static int cordic_ccos (lua_State *L) {
  lua_Number c, s;
  int n = agnL_optposint(L, 2, ANGLES_LENGTH_ITERS);  /* n = 27 is great, with just two deviations from Sun's sin implementation */
  cossin_cordic(agn_checknumber(L, 1), n, &c, &s);
  lua_pushnumber(L, c);
  return 1;
}


/*
  Purpose: TAN_CORDIC returns the tangent of an angle using the CORDIC method.

  Modified: 19 January 2012

  Author:

    Based on MATLAB code in a Wikipedia article.
    C++ version by John Burkardt

  Parameters:

    Input, double BETA, the angle (in radians).
    Input, int N, the number of iterations to take. (A value of 10 is low.
      Good accuracy is achieved with 20 or more iterations.)

    Output, double TAN_CORDIC, the tangent of the angle.

  Local Parameters:

    Local, double ANGLES(60) = arctan ((1/2)^(0:59));
*/

double tan_cordic (double x, int n) {
  int i;
  double angle, c, c2, factor, poweroftwo, s, s2, sigma, t, theta;
  /* shift angle to interval [-PI, PI] */
  theta = angle_shift( x, -PI);
  /* shift angle to interval [-PI/2, PI/2] */
  if (theta < - 0.5 * PI)
    theta += PI;
  else if (0.5 * PI < theta)
    theta -= PI;
  c = 1;
  s = 0;
  poweroftwo = 1;
  angle = TRIG_CORDIC_angles[0];
  for (i=1; i <= n; i++) {
    sigma = (theta < 0) ? -1 : 1;
    factor = sigma * poweroftwo;
    c2 =          c - factor * s;
    s2 = factor * c +          s;
    c = c2;
    s = s2;
    /* update the remaining angle */
    theta -= sigma * angle;
    poweroftwo /= 2;
    /* update the angle from table, or eventually by just dividing by two */
    angle = (ANGLES_LENGTH_ITERS < i + 1) ? angle / 2 : TRIG_CORDIC_angles[i];
  }
  t = s / c;
  return t;
}

static int cordic_ctan (lua_State *L) {
  int n = agnL_optposint(L, 2, ANGLES_LENGTH_ITERS);
  lua_pushnumber(L, tan_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/*
  Purpose: EXP_CORDIC evaluates the exponential function using the CORDIC method.

  Modified: 19 January 2012

  Author: John Burkardt [2]

  Parameters:

    Input, double X, the argument.
    Input, int N, the number of steps to take.
    Output, double FX, the exponential of X.

  Local Parameters:

    Local, double A(1:25) = exp((1/2)^(1:25));
*/

double exp_cordic (double x, int n) {
  int i, x_int, isinfin, isneg;
  double ai, fx, z, poweroftwo, *w;
  isinfin = tools_isinfx(x, &isneg);
  if (isinfin && isneg) return 0;           /* 4.11.5 fix for -infinity */
  if (tools_isnan(x) || isinfin) return x;  /* 4.11.5 fix for undefined, +infinity*/
  x_int = (int)(floor(x));
  /* determine the weights */
  poweroftwo = 0.5;
  z = x - (double)(x_int);
  w = (double *)malloc(n*sizeof(double));
  if (w == NULL) return AGN_NAN;  /* 4.11.5 fix */
  for (i=0; i < n; i++) {
    w[i] = 0;
    if (poweroftwo < z) {
      w[i] = 1;
      z -= poweroftwo;
    }
    poweroftwo *= 0.5;
  }
  /* calculate products */
  ai = fx = 1;
  for (i=0; i < n; i++) {
    ai = (i < EXPLNA_LENGTH_ITERS) ? EXPLN_CORDIC_a[i] : 1 + 0.5*(ai - 1);
    if (0 < w[i]) fx *= ai;
  }
  /* perform residual multiplication */
  fx = fx
    * ( 1 + z
    * ( 1 + z / 2
    * ( 1 + z / 3
    * ( 1 + z / 4))));
  /* account for factor EXP(X_INT) */
  if (x_int < 0) {
    for (i=1; i <= -x_int; i++) fx /= EXP1;
  } else {
    for (i=1; i <= x_int; i++) fx *= EXP1;
  }
  xfree(w);
  return fx;
}

static int cordic_cexp (lua_State *L) {
  int n = agnL_optposint(L, 2, EXPLNA_LENGTH_ITERS);
  lua_pushnumber(L, exp_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/*
  Purpose: LN_CORDIC evaluates the natural logarithm using the CORDIC method.

  Modified: 19 January 2012

  Author: John Burkardt [2]

  Parameters:

    Input, double X, the argument.
    Input, int N, the number of steps to take.
    Output, double FX, the natural logarithm of X.

  Local Parameters:

    Local, double A(1:25) = exp((1/2)^(1:25));
*/

double ln_cordic (double x, int n) {
  double ai, fx, poweroftwo, *w;
  int i, k, isinfin, isneg;
  isinfin = tools_isinfx(x, &isneg);
  if (isinfin) return (isneg) ? AGN_NAN : x;  /* 4.11.5 fix for +infinity */
  if (isneg || x == 0.0 || tools_isnan(x)) return AGN_NAN;  /* 4.11.5 fix for undefined */
  k = ai = 0;
  while (EXP1 <= x) {
    k++;
    x /= EXP1;
  }
  while (x < 1) {
    k--;
    x *= EXP1;
  }
  /* determine the weights */
  w = (double *)malloc(n*sizeof(double));
  if (w == NULL) return AGN_NAN;  /* 4.11.5 fix */
  for (i=0; i < n; i++) {
    w[i] = 0;
    ai = (i < EXPLNA_LENGTH_ITERS) ? EXPLN_CORDIC_a[i] : 1 + 0.5*(ai - 1);
    if (ai < x) {
      w[i] = 1;
      x = x / ai;
    }
  }
  x = x - 1;
  x = x
    * (1 - (x / 2)
    * (1 + (x / 3)
    * (1 -  x / 4)));
  /* assemble */
  poweroftwo = 0.5;
  for (i=0; i < n; i++) {
    x += w[i] * poweroftwo;
    poweroftwo *= 0.5;
  }
  fx = x + (double)(k);
  xfree(w);
  return fx;
}

static int cordic_cln (lua_State *L) {
  int n = agnL_optposint(L, 2, EXPLNA_LENGTH_ITERS);
  lua_pushnumber(L, ln_cordic(agn_checknumber(L, 1), n));
  return 1;
}


/* in Maple V Release 4 use: convert(<trigfunc>(x), exp) */

static int cordic_csinh (lua_State *L) {
  lua_Number e;
  int n = agnL_optposint(L, 2, EXPLNA_LENGTH_ITERS);
  e = exp_cordic(agn_checknumber(L, 1), n);
  lua_pushnumber(L, 0.5*e - 0.5/e);
  return 1;
}


static int cordic_ccosh (lua_State *L) {
  lua_Number e;
  int n = agnL_optposint(L, 2, EXPLNA_LENGTH_ITERS);
  e = exp_cordic(agn_checknumber(L, 1), n);
  lua_pushnumber(L, 0.5*e + 0.5/e);
  return 1;
}


static int cordic_ctanh (lua_State *L) {
  lua_Number e, e2;
  int n = agnL_optposint(L, 2, EXPLNA_LENGTH_ITERS);
  e = exp_cordic(agn_checknumber(L, 1), n);
  e2 = e * e;
  lua_pushnumber(L, (e2 - 1)/(e2 + 1));
  return 1;
}


static int cordic_carctanh (lua_State *L) {
  lua_Number x;
  int n = agnL_optposint(L, 2, EXPLNA_LENGTH_ITERS);
  x = agn_checknumber(L, 1);
  lua_pushnumber(L, 0.5*(ln_cordic(x + 1, n) - ln_cordic(1 - x, n)));
  return 1;
}


/******************************************************************************/
/*
  Purpose:

    MULTIPLY_CORDIC computes Z=X*Y the CORDIC method.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    27 June 2018

  Author:

    John Burkardt

  Reference:

    Jean-Michel Muller,
    Elementary Functions: Algorithms and Implementation,
    Second Edition,
    Birkhaeuser, 2006,
    ISBN13: 978-0-8176-4372-0,
    LC: QA331.M866.

  Parameters:

    Input, double X, Y, the factors.

    Output, double Z, the product.

  Modifications:

    Just some few simplifications in the code for Agena. 2.14.9 */

double multiply_cordic (double x, double y) {
  double two_power, x_abs, x_log2, x_sign, z;
  z = 0.0;
  /* Easy result if X or Y is zero. */
  if (x == 0.0 || y == 0.0) return z;
  /* X = X_SIGN * X_ABS. */
  if (x < 0.0) {
    x_sign = -1.0;
    x_abs = -x;
  } else {
    x_sign = 1.0;
    x_abs = x;
  }
  /* X = X_SIGN * X_ABS * 2^X_LOG2 */
  x_log2 = 0;
  while (2.0 <= x_abs) {
    x_abs *= 0.5;
    x_log2 += 1;
  }
  while (x_abs < 1.0) {
    x_abs *= 2.0;
    x_log2 -= 1;
  }
  /* X*Y = X_SIGN * sum (0 <= i) X_ABS(i) * 2^(-i) * Y) * 2^X_LOG2
    where X_ABS(I) is the I-th binary digit in expansion of X_ABS. */
  two_power = 1.0;
  while (0.0 < x_abs) {
    if (1.0 <= x_abs) {
      x_abs -= 1.0;
      z += y * two_power;
    }
    x_abs *= 2.0;
    two_power *= 0.5;
  }
  /* Account for X_SIGN and X_LOG2. */
  z = z * x_sign;
  while (0 < x_log2) {
    z *= 2.0;
    x_log2 -= 1;
  }
  while (x_log2 < 0) {
    z *= 0.5;
    x_log2 += 1;
  }
  return z;
}

static int cordic_cmul (lua_State *L) {
  lua_pushnumber(L, multiply_cordic(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int cordic_cpow (lua_State *L) {
  lua_Number x = agn_checknumber(L, 1);
  lua_Number e = agn_checknumber(L, 2);
  int n = agnL_optposint(L, 3, EXPLNA_LENGTH_ITERS);
  lua_pushnumber(L, x == 0.0 && e != 0.0 ? 0.0 : exp_cordic(e*ln_cordic(x, n), n));
  return 1;
}


#undef KPROD_LENGTH_ITERS
#undef EXPLNA_LENGTH_ITERS
#undef ANGLES_LENGTH_ITERS

#undef KPROD_LENGTH
#undef EXPLNA_LENGTH
#undef ANGLES_LENGTH


static const luaL_Reg cordiclib[] = {
  {"carccos", cordic_carccos},    /* added on April 05, 2014 */
  {"carcsin", cordic_carcsin},    /* added on April 05, 2014 */
  {"carctan2", cordic_carctan2},  /* added on April 05, 2014 */
  {"carctanh", cordic_carctanh},  /* added on April 06, 2014 */
  {"ccbrt", cordic_ccbrt},        /* added on April 05, 2014 */
  {"ccos", cordic_ccos},          /* added on April 05, 2014 */
  {"ccosh", cordic_ccosh},        /* added on April 05, 2014 */
  {"cexp", cordic_cexp},          /* added on April 05, 2014 */
  {"chypot", cordic_chypot},      /* added on April 06, 2014 */
  {"cln", cordic_cln},            /* added on April 05, 2014 */
  {"cmul", cordic_cmul},          /* added on April 14, 2019 */
  {"cpow", cordic_cpow},          /* added on April 20, 2025 */
  {"csin", cordic_csin},          /* added on April 05, 2014 */
  {"csinh", cordic_csinh},        /* added on April 05, 2014 */
  {"csqrt", cordic_csqrt},        /* added on April 05, 2014 */
  {"ctan", cordic_ctan},          /* added on April 05, 2014 */
  {"ctanh", cordic_ctanh},        /* added on April 05, 2014 */
  {NULL, NULL}
};

/*
** Open cordic library
*/

LUALIB_API int luaopen_cordic (lua_State *L) {
  luaL_register(L, AGENA_CORDICLIBNAME, cordiclib);
  lua_rawsetstringnumber(L, -1, "gain", GAIN);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  return 1;
}

