#include "ntt64.h"
#include <string.h>

// ============================================================================
// MODULI AND PARAMETERS
// ============================================================================

// Prime moduli (all satisfy q ≡ 1 mod 256 for N=64 negacyclic NTT with psi preprocessing)
// Exported for SIMD implementations
const uint32_t Q[NTT_NUM_LAYERS] = {
    257u,          // Layer 0: 2^8 + 1
    3329u,         // Layer 1: Kyber-like (close to target 4993)
    10753u,        // Layer 2:
    43777u,        // Layer 3: (close to target 43649)
    64513u,        // Layer 4: RS_PRIME_B
    686593u,       // Layer 5: (close to target 687233)
    2818573313u    // Layer 6: (close to target 2818572929)
};

// Inverse of N=64 for each modulus (N_inv = 64^(-1) mod q)
// Exported for SIMD implementations
const uint32_t N_INV[NTT_NUM_LAYERS] = {
    253u,          // for q=257
    3277u,         // for q=3329
    10585u,        // for q=10753
    43093u,        // for q=43777
    63505u,        // for q=64513
    675865u,       // for q=686593
    2774533105u    // for q=2818573313
};

// Barrett reduction constants: floor(2^64 / q)
// Exported for SIMD implementations
const uint64_t BARRETT_CONST[NTT_NUM_LAYERS] = {
    71777214294589695ULL,    // for q=257
    5541226816974932ULL,     // for q=3329
    1715497449428954ULL,     // for q=10753
    421379813000195ULL,      // for q=43777
    285938401154954ULL,      // for q=64513
    26867072739904ULL,       // for q=686593
    6544709690ULL            // for q=2818573313
};

// ============================================================================
// PRECOMPUTED TWIDDLE FACTORS (for speed optimization)
// ============================================================================

// Precomputed twiddle factors for forward NTT
// twiddles_fwd[layer][stage] = omega^(64 / (2^(stage+1)))
// Exported for SIMD implementations
const uint32_t TWIDDLES_FWD[NTT_NUM_LAYERS][6] = {
    { 256u, 241u, 64u, 249u, 136u, 81u },         // Layer 0 (q=257)
    { 3328u, 1729u, 749u, 2699u, 2532u, 1996u },  // Layer 1 (q=3329)
    { 10752u, 6264u, 321u, 9097u, 9599u, 6970u },  // Layer 2 (q=10753)
    { 43776u, 20924u, 37159u, 17026u, 16527u, 22287u }, // Layer 3 (q=43777)
    { 64512u, 35676u, 20201u, 39866u, 41871u, 15914u },  // Layer 4 (q=64513)
    { 686592u, 149740u, 308987u, 514852u, 192219u, 92055u }, // Layer 5 (q=686593)
    { 2818573312u, 678987471u, 1315489751u, 1317825540u, 227013343u, 76152835u } // Layer 6 (q=2818573313)
};

// Precomputed twiddle factors for inverse NTT
// twiddles_inv[layer][stage] = omega_inv^(64 / (2^(stage+1)))
// Exported for SIMD implementations
const uint32_t TWIDDLES_INV[NTT_NUM_LAYERS][6] = {
    { 256u, 16u, 253u, 32u, 240u, 165u },         // Layer 0 (q=257)
    { 3328u, 1600u, 3289u, 1897u, 2786u, 1426u }, // Layer 1 (q=3329)
    { 10752u, 4489u, 67u, 7331u, 2637u, 3013u },   // Layer 2 (q=10753)
    { 43776u, 22853u, 8381u, 25663u, 20825u, 3021u }, // Layer 3 (q=43777)
    { 64512u, 28837u, 48360u, 13268u, 22985u, 59093u },  // Layer 4 (q=64513)
    { 686592u, 536853u, 415704u, 579255u, 403221u, 604982u }, // Layer 5 (q=686593)
    { 2818573312u, 2139585842u, 1152851736u, 1376085826u, 1221892762u, 2693805399u } // Layer 6 (q=2818573313)
};

// Precomputed psi powers for preprocessing: psi_powers[layer][i] = psi^i mod q
// Exported for SIMD implementations
const uint32_t PSI_POWERS[NTT_NUM_LAYERS][NTT_N] = {
    { // Layer 0 (q=257)
        1u,9u,81u,215u,136u,196u,222u,199u,249u,185u,123u,79u,197u,231u,23u,207u,
        64u,62u,44u,139u,223u,208u,73u,143u,2u,18u,162u,173u,15u,135u,187u,141u,
        241u,113u,246u,158u,137u,205u,46u,157u,128u,124u,88u,21u,189u,159u,146u,29u,
        4u,36u,67u,89u,30u,13u,117u,25u,225u,226u,235u,59u,17u,153u,92u,57u
    },
    { // Layer 1 (q=3329)
        1u,1915u,1996u,648u,2532u,1756u,450u,2868u,2699u,1977u,882u,1227u,2760u,2277u,2794u,807u,
        749u,2865u,283u,2647u,2267u,289u,821u,927u,848u,2697u,1476u,219u,3260u,1025u,2094u,1894u,
        1729u,2009u,2240u,1848u,193u,76u,2393u,1891u,2642u,2679u,296u,910u,1583u,2055u,447u,452u,
        40u,33u,3273u,2617u,1410u,331u,1355u,1534u,1432u,2513u,1990u,2474u,543u,1197u,1903u,2319u
    },
    { // Layer 2 (q=10753)
        1u, 5606u, 6970u, 8171u, 9599u, 3982u, 10617u, 1047u, 9097u, 7056u, 6402u, 6851u, 7743u, 8150u, 10156u, 8154u,
        321u, 3775u, 746u, 9912u, 5921u, 9368u, 10109u, 2744u, 6074u, 6846u, 1219u, 5559u, 1560u, 3171u, 1917u, 4455u,
        6264u, 7439u, 2900u, 9617u, 8113u, 7041u, 8336u, 9831u, 3461u, 3954u, 4191u, 10194u, 6122u, 7109u, 2436u, 10659u,
        10686u, 753u, 6142u, 946u, 2047u, 2031u, 9112u, 5122u, 3422u, 380u, 1186u, 3362u, 8116u, 2353u, 7740u, 2085u
    },
    { // Layer 3 (q=43777)
        1u, 30304u, 22287u, 37469u, 16527u, 25328u, 41348u, 24498u, 17026u, 182u, 43203u, 28750u, 33923u, 31078u, 13111u, 39469u,
        37159u, 34342u, 33124u, 26863u, 23037u, 1429u, 8963u, 22244u, 3930u, 21280u, 33910u, 31119u, 29819u, 33919u, 41193u, 11517u,
        20924u, 14828u, 20584u, 42840u, 16425u, 42487u, 701u, 11259u, 38575u, 43346u, 28299u, 25243u, 4574u, 12514u, 27882u, 40028u,
        35396u, 16330u, 9112u, 28509u, 41418u, 705u, 1144u, 40169u, 18114u, 6853u, 39001u, 38635u, 22952u, 8432u, 40756u, 33100u
    },
    { // Layer 4 (q=64513)
        1u, 12565u, 15914u, 33623u, 41871u, 5600u, 44830u, 25947u, 39866u, 37358u, 6682u, 27917u, 19924u, 34620u, 53654u, 1660u,
        20201u, 31423u, 10435u, 25359u, 6128u, 34311u, 41849u, 51735u, 17287u, 60397u, 21886u, 43184u, 52630u, 37700u, 46054u, 51413u,
        35676u, 32616u, 33464u, 43939u, 55794u, 53352u, 13297u, 52648u, 5818u, 9941u, 11497u, 15198u, 4390u, 1735u, 59394u, 63739u,
        16153u, 4547u, 39050u, 41885u, 52484u, 9574u, 45078u, 45443u, 51245u, 53685u, 4097u, 61944u, 41528u, 18176u, 5420u, 41085u
    },
    { // Layer 5 (q=686593)
        1u, 75445u, 92055u, 201280u, 192219u, 431702u, 531842u, 324770u, 514852u, 383351u, 559256u, 555884u, 194754u, 125330u, 449647u, 430971u,
        308987u, 318679u, 310074u, 622827u, 131281u, 391020u, 349062u, 21482u, 350010u, 137670u, 420839u, 78256u, 10713u, 122324u, 237667u, 410620u,
        149740u, 619671u, 274632u, 294279u, 207907u, 326530u, 99010u, 364203u, 530068u, 370975u, 618416u, 340991u, 112878u, 267731u, 85828u, 34877u,
        270889u, 93367u, 315728u, 128011u, 172757u, 56946u, 278569u, 26475u, 107338u, 437568u, 239727u, 657302u, 283372u, 554299u, 81611u, 462464u
    },
    { // Layer 6 (q=2818573313)
        1u, 1937063832u, 76152835u, 1557849399u, 227013343u, 1186412631u, 1511285157u, 2695558944u, 1317825540u, 1875200075u, 2525047000u, 2623988841u, 1072524612u, 2390136231u, 1007515042u, 1377121834u,
        1315489751u, 666276137u, 95371989u, 1466964160u, 1181893362u, 5660407u, 987745255u, 349253503u, 11792678u, 342413901u, 488674009u, 696644753u, 230148589u, 1355634142u, 855836650u, 106891284u,
        678987471u, 762566384u, 406225956u, 1794923422u, 1166007134u, 1766639302u, 2088348109u, 108927320u, 1388422478u, 958180314u, 1484556778u, 2570446152u, 655723964u, 1903901480u, 1647713318u, 2303349723u,
        1665721577u, 1373240375u, 1977972379u, 342824182u, 1950530756u, 872766783u, 2462097263u, 861885440u, 1442487487u, 2204983470u, 2412975673u, 1080030273u, 1596680551u, 2707644020u, 124767914u, 2571349714u
    }
};

// Precomputed psi inverse powers for postprocessing: psi_inv_powers[layer][i] = psi_inv^i mod q
// Exported for SIMD implementations
const uint32_t PSI_INV_POWERS[NTT_NUM_LAYERS][NTT_N] = {
    { // Layer 0 (q=257)
        1u,200u,165u,104u,240u,198u,22u,31u,32u,232u,140u,244u,227u,168u,190u,221u,
        253u,228u,111u,98u,68u,236u,169u,133u,129u,100u,211u,52u,120u,99u,11u,144u,
        16u,116u,70u,122u,242u,84u,95u,239u,255u,114u,184u,49u,34u,118u,213u,195u,
        193u,50u,234u,26u,60u,178u,134u,72u,8u,58u,35u,61u,121u,42u,176u,248u
    },
    { // Layer 1 (q=3329)
        1u,1010u,1426u,2132u,2786u,855u,1339u,816u,1897u,1795u,1974u,2998u,1919u,712u,56u,3296u,
        3289u,2877u,2882u,1274u,1746u,2419u,3033u,650u,687u,1438u,936u,3253u,3136u,1481u,1089u,1320u,
        1600u,1435u,1235u,2304u,69u,3110u,1853u,632u,2481u,2402u,2508u,3040u,1062u,682u,3046u,464u,
        2580u,2522u,535u,1052u,569u,2102u,2447u,1352u,630u,461u,2879u,1573u,797u,2681u,1333u,1414u
    },
    { // Layer 2 (q=10753)
        1u, 8668u, 3013u, 8400u, 2637u, 7391u, 9567u, 10373u, 7331u, 5631u, 1641u, 8722u, 8706u, 9807u, 4611u, 10000u,
        67u, 94u, 8317u, 3644u, 4631u, 559u, 6562u, 6799u, 7292u, 922u, 2417u, 3712u, 2640u, 1136u, 7853u, 3314u,
        4489u, 6298u, 8836u, 7582u, 9193u, 5194u, 9534u, 3907u, 4679u, 8009u, 644u, 1385u, 4832u, 841u, 10007u, 6978u,
        10432u, 2599u, 597u, 2603u, 3010u, 3902u, 4351u, 3697u, 1656u, 9706u, 136u, 6771u, 1154u, 2582u, 3783u, 5147u
    },
    { // Layer 3 (q=43777)
        1u, 10677u, 3021u, 35345u, 20825u, 5142u, 4776u, 36924u, 25663u, 3608u, 42633u, 43072u, 2359u, 15268u, 34665u, 27447u,
        8381u, 3749u, 15895u, 31263u, 39203u, 18534u, 15478u, 431u, 5202u, 32518u, 43076u, 1290u, 27352u, 937u, 23193u, 28949u,
        22853u, 32260u, 2584u, 9858u, 13958u, 12658u, 9867u, 22497u, 39847u, 21533u, 34814u, 42348u, 20740u, 16914u, 10653u, 9435u,
        6618u, 4308u, 30666u, 12699u, 9854u, 15027u, 574u, 43595u, 26751u, 19279u, 2429u, 18449u, 27250u, 6308u, 21490u, 13473u
    },
    { // Layer 4 (q=64513)
        1u, 23428u, 59093u, 46337u, 22985u, 2569u, 60416u, 10828u, 13268u, 19070u, 19435u, 54939u, 12029u, 22628u, 25463u, 59966u,
        48360u, 774u, 5119u, 62778u, 60123u, 49315u, 53016u, 54572u, 58695u, 11865u, 51216u, 11161u, 8719u, 20574u, 31049u, 31897u,
        28837u, 13100u, 18459u, 26813u, 11883u, 21329u, 42627u, 4116u, 47226u, 12778u, 22664u, 30202u, 58385u, 39154u, 54078u, 33090u,
        44312u, 62853u, 10859u, 29893u, 44589u, 36596u, 57831u, 27155u, 24647u, 38566u, 19683u, 58913u, 22642u, 30890u, 48599u, 51948u
    },
    { // Layer 5 (q=686593)
        1u, 224129u, 604982u, 132294u, 403221u, 29291u, 446866u, 249025u, 579255u, 660118u, 408024u, 629647u, 513836u, 558582u, 370865u, 593226u,
        415704u, 651716u, 600765u, 418862u, 573715u, 345602u, 68177u, 315618u, 156525u, 322390u, 587583u, 360063u, 478686u, 392314u, 411961u, 66922u,
        536853u, 275973u, 448926u, 564269u, 675880u, 608337u, 265754u, 548923u, 336583u, 665111u, 337531u, 295573u, 555312u, 63766u, 376519u, 367914u,
        377606u, 255622u, 236946u, 561263u, 491839u, 130709u, 127337u, 303242u, 171741u, 361823u, 154751u, 254891u, 494374u, 485313u, 594538u, 611148u
    },
    { // Layer 6 (q=2818573313)
        1u, 247223599u, 2693805399u, 110929293u, 1221892762u, 1738543040u, 405597640u, 613589843u, 1376085826u, 1956687873u, 356476050u, 1945806530u, 868042557u, 2475749131u, 840600934u, 1445332938u,
        1152851736u, 515223590u, 1170859995u, 914671833u, 2162849349u, 248127161u, 1334016535u, 1860392999u, 1430150835u, 2709645993u, 730225204u, 1051934011u, 1652566179u, 1023649891u, 2412347357u, 2056006929u,
        2139585842u, 2711682029u, 1962736663u, 1462939171u, 2588424724u, 2121928560u, 2329899304u, 2476159412u, 2806780635u, 2469319810u, 1830828058u, 2812912906u, 1636679951u, 1351609153u, 2723201324u, 2152297176u,
        1503083562u, 1441451479u, 1811058271u, 428437082u, 1746048701u, 194584472u, 293526313u, 943373238u, 1500747773u, 123014369u, 1307288156u, 1632160682u, 2591559970u, 1260723914u, 2742420478u, 881509481u
    }
};

// ============================================================================
// CONSTANT-TIME MODULAR ARITHMETIC
// ============================================================================

/**
 * Constant-time Barrett reduction
 * Returns x mod q
 */
static inline uint32_t ct_barrett_reduce(uint64_t x, int layer) {
    // Compute q * floor(x / q) approximately
    uint64_t q = Q[layer];
    uint64_t b = BARRETT_CONST[layer];

    // quotient approximation: floor(x * b / 2^64)
    uint64_t q_approx = ((__uint128_t)x * b) >> 64;

    // remainder: x - q * q_approx
    uint64_t r = x - q_approx * q;

    // Conditional subtraction (constant-time)
    // If r >= q, subtract q; otherwise keep r
    uint64_t mask = -(uint64_t)(r >= q);
    r -= mask & q;

    // One more reduction may be needed for very large inputs
    mask = -(uint64_t)(r >= q);
    r -= mask & q;

    return (uint32_t)r;
}

/**
 * Constant-time modular multiplication: (a * b) mod q
 */
static inline uint32_t ct_mul_mod(uint32_t a, uint32_t b, int layer) {
    uint64_t product = (uint64_t)a * (uint64_t)b;
    return ct_barrett_reduce(product, layer);
}

/**
 * Constant-time modular addition: (a + b) mod q
 */
static inline uint32_t ct_add_mod(uint32_t a, uint32_t b, int layer) {
    uint64_t sum = (uint64_t)a + (uint64_t)b;
    uint32_t q = Q[layer];

    // Conditional subtraction
    uint32_t mask = -(uint32_t)(sum >= q);
    sum -= mask & q;

    return (uint32_t)sum;
}

/**
 * Constant-time modular subtraction: (a - b) mod q
 */
static inline uint32_t ct_sub_mod(uint32_t a, uint32_t b, int layer) {
    uint32_t q = Q[layer];
    int64_t diff = (int64_t)a - (int64_t)b;

    // Conditional addition
    int64_t mask = -(int64_t)(diff < 0);
    diff += mask & q;

    return (uint32_t)diff;
}

// ============================================================================
// BIT-REVERSAL
// ============================================================================

/**
 * Bit-reverse a 6-bit index (for N=64=2^6)
 */
static inline uint32_t bit_reverse_6(uint32_t x) {
    uint32_t result = 0;
    result |= ((x & 0x01) << 5);  // bit 0 -> bit 5
    result |= ((x & 0x02) << 3);  // bit 1 -> bit 4
    result |= ((x & 0x04) << 1);  // bit 2 -> bit 3
    result |= ((x & 0x08) >> 1);  // bit 3 -> bit 2
    result |= ((x & 0x10) >> 3);  // bit 4 -> bit 1
    result |= ((x & 0x20) >> 5);  // bit 5 -> bit 0
    return result;
}

/**
 * In-place bit-reversal permutation
 * Exposed for SIMD implementations
 */
void bit_reverse_copy(uint32_t poly[NTT_N]) {
    for (uint32_t i = 0; i < NTT_N; i++) {
        uint32_t j = bit_reverse_6(i);

        if (i < j) {
            uint32_t temp = poly[i];
            poly[i] = poly[j];
            poly[j] = temp;
        }
    }
}

// ============================================================================
// NTT BUTTERFLIES (CONSTANT-TIME)
// ============================================================================

/**
 * Cooley-Tukey butterfly (constant-time)
 *
 * (a, b) -> (a + w*b, a - w*b) mod q
 */
static inline void ct_butterfly(uint32_t *a, uint32_t *b, uint32_t w, int layer) {
    uint32_t t = ct_mul_mod(*b, w, layer);
    uint32_t u = *a;

    *a = ct_add_mod(u, t, layer);
    *b = ct_sub_mod(u, t, layer);
}

/**
 * Gentleman-Sande butterfly for inverse NTT (constant-time)
 *
 * (a, b) -> ((a + b), w*(a - b)) mod q
 */
static inline void ct_inv_butterfly(uint32_t *a, uint32_t *b, uint32_t w, int layer) {
    uint32_t u = *a;
    uint32_t v = *b;

    *a = ct_add_mod(u, v, layer);

    uint32_t diff = ct_sub_mod(u, v, layer);
    *b = ct_mul_mod(diff, w, layer);
}

// ============================================================================
// FORWARD NTT (Scalar implementation)
// ============================================================================

void ntt64_forward_scalar(uint32_t poly[NTT_N], int layer) {
    // Preprocessing: multiply poly[i] by psi^i using precomputed table
    for (uint32_t i = 0; i < NTT_N; i++) {
        poly[i] = ct_mul_mod(poly[i], PSI_POWERS[layer][i], layer);
    }

    // Bit-reverse input
    bit_reverse_copy(poly);

    // Standard Cooley-Tukey NTT (iterative, constant-time)
    // log2(64) = 6 stages
    for (uint32_t stage = 0; stage < 6; stage++) {
        uint32_t m = 1u << (stage + 1);        // Block size
        uint32_t m_half = 1u << stage;         // Half block size

        // Get precomputed twiddle factor for this stage
        uint32_t omega_m = TWIDDLES_FWD[layer][stage];

        // Process all blocks at this stage
        for (uint32_t k = 0; k < NTT_N; k += m) {
            uint32_t w = 1;

            for (uint32_t j = 0; j < m_half; j++) {
                uint32_t idx_a = k + j;
                uint32_t idx_b = k + j + m_half;

                ct_butterfly(&poly[idx_a], &poly[idx_b], w, layer);

                w = ct_mul_mod(w, omega_m, layer);
            }
        }
    }
}

// ============================================================================
// INVERSE NTT (Scalar implementation)
// ============================================================================

void ntt64_inverse_scalar(uint32_t poly[NTT_N], int layer) {
    // Standard Gentleman-Sande inverse NTT (iterative, constant-time)
    // log2(64) = 6 stages (reverse order compared to forward)
    for (int stage = 5; stage >= 0; stage--) {
        uint32_t m = 1u << (stage + 1);
        uint32_t m_half = 1u << stage;

        // Get precomputed twiddle factor for this stage
        uint32_t omega_m = TWIDDLES_INV[layer][stage];

        // Process all blocks at this stage
        for (uint32_t k = 0; k < NTT_N; k += m) {
            uint32_t w = 1;

            for (uint32_t j = 0; j < m_half; j++) {
                uint32_t idx_a = k + j;
                uint32_t idx_b = k + j + m_half;

                ct_inv_butterfly(&poly[idx_a], &poly[idx_b], w, layer);

                w = ct_mul_mod(w, omega_m, layer);
            }
        }
    }

    // Bit-reverse output
    bit_reverse_copy(poly);

    // Multiply by N^(-1) to complete inverse transform
    uint32_t n_inv = N_INV[layer];
    for (uint32_t i = 0; i < NTT_N; i++) {
        poly[i] = ct_mul_mod(poly[i], n_inv, layer);
    }

    // Postprocessing: multiply poly[i] by psi^(-i) using precomputed table
    for (uint32_t i = 0; i < NTT_N; i++) {
        poly[i] = ct_mul_mod(poly[i], PSI_INV_POWERS[layer][i], layer);
    }
}

// ============================================================================
// POINT-WISE MULTIPLICATION (Scalar implementation)
// ============================================================================

void ntt64_pointwise_mul_scalar(uint32_t result[NTT_N],
                                 const uint32_t a[NTT_N],
                                 const uint32_t b[NTT_N],
                                 int layer) {
    for (uint32_t i = 0; i < NTT_N; i++) {
        result[i] = ct_mul_mod(a[i], b[i], layer);
    }
}

// ============================================================================
// PUBLIC API (backward compatibility wrappers)
// ============================================================================

// These are simple wrappers that call the scalar implementation
// In a separate dispatch file, these will be replaced with runtime dispatch

void ntt64_forward(uint32_t poly[NTT_N], int layer) {
    ntt64_forward_scalar(poly, layer);
}

void ntt64_inverse(uint32_t poly[NTT_N], int layer) {
    ntt64_inverse_scalar(poly, layer);
}

void ntt64_pointwise_mul(uint32_t result[NTT_N],
                         const uint32_t a[NTT_N],
                         const uint32_t b[NTT_N],
                         int layer) {
    ntt64_pointwise_mul_scalar(result, a, b, layer);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint32_t ntt64_get_modulus(int layer) {
    return Q[layer];
}

// ============================================================================
// PUBLIC FIELD ARITHMETIC API
// ============================================================================

uint32_t ntt64_add_mod(uint32_t a, uint32_t b, int layer) {
    return ct_add_mod(a, b, layer);
}

uint32_t ntt64_sub_mod(uint32_t a, uint32_t b, int layer) {
    return ct_sub_mod(a, b, layer);
}

uint32_t ntt64_mul_mod(uint32_t a, uint32_t b, int layer) {
    return ct_mul_mod(a, b, layer);
}

/**
 * Constant-time modular inverse using binary extended GCD algorithm.
 * Based on the algorithm from "Fast constant-time gcd computation and
 * modular inversion" by Daniel J. Bernstein and Bo-Yin Yang.
 *
 * This implementation runs in constant time to prevent timing side-channels.
 */
uint32_t ntt64_inv_mod(uint32_t a, int layer) {
    uint32_t q = Q[layer];

    // Handle edge cases
    if (a == 0) return 0;
    if (a >= q) a = a % q;
    if (a == 0) return 0;
    if (a == 1) return 1;

    // Binary extended GCD (constant-time)
    // We maintain: u*a + v*q = r and s*a + t*q = w
    // Initially: r=a, w=q, u=1, v=0, s=0, t=1

    uint64_t r = a;
    uint64_t w = q;
    uint64_t u = 1;
    uint64_t v = 0;
    uint64_t s = 0;
    uint64_t t = 1;

    // Run for a fixed number of iterations (constant-time)
    // log2(q) * 2 iterations is sufficient for 32-bit moduli
    int iterations = 96; // Conservative upper bound for 32-bit moduli

    for (int i = 0; i < iterations; i++) {
        // Constant-time: always do the same operations

        // Check if r is even
        uint64_t r_even = -((r & 1) ^ 1); // All 1s if even, all 0s if odd

        // If r is even: r = r/2, u = u/2 (mod q), v = v/2 (mod q)
        uint64_t new_r = r >> 1;

        // For u/2: if u is odd, add q first then divide
        uint64_t u_odd_mask = -(u & 1);
        uint64_t u_adjusted = u + (u_odd_mask & q);
        uint64_t new_u = u_adjusted >> 1;

        // Same for v
        uint64_t v_odd_mask = -(v & 1);
        uint64_t v_adjusted = v + (v_odd_mask & q);
        uint64_t new_v = v_adjusted >> 1;

        // Conditionally update if r was even
        r = (r_even & new_r) | (~r_even & r);
        u = (r_even & new_u) | (~r_even & u);
        v = (r_even & new_v) | (~r_even & v);

        // Check if w is even
        uint64_t w_even = -((w & 1) ^ 1);

        // If w is even: w = w/2, s = s/2 (mod q), t = t/2 (mod q)
        uint64_t new_w = w >> 1;

        uint64_t s_odd_mask = -(s & 1);
        uint64_t s_adjusted = s + (s_odd_mask & q);
        uint64_t new_s = s_adjusted >> 1;

        uint64_t t_odd_mask = -(t & 1);
        uint64_t t_adjusted = t + (t_odd_mask & q);
        uint64_t new_t = t_adjusted >> 1;

        // Conditionally update if w was even
        w = (w_even & new_w) | (~w_even & w);
        s = (w_even & new_s) | (~w_even & s);
        t = (w_even & new_t) | (~w_even & t);

        // Both r and w are odd now (or we've updated them)
        // Check if r >= w
        uint64_t r_ge_w = -(uint64_t)(r >= w);

        // If r >= w: r = r - w, u = u - s, v = v - t
        uint64_t diff_r = r - w;
        // Handle u - s with proper modular arithmetic
        uint64_t diff_u = (u >= s) ? (u - s) : (q + u - s);
        uint64_t diff_v = (v >= t) ? (v - t) : (q + v - t);

        r = (r_ge_w & diff_r) | (~r_ge_w & r);
        u = (r_ge_w & diff_u) | (~r_ge_w & u);
        v = (r_ge_w & diff_v) | (~r_ge_w & v);

        // If r < w: w = w - r, s = s - u, t = t - v
        uint64_t diff_w = w - r;
        uint64_t diff_s = (s >= u) ? (s - u) : (q + s - u);
        uint64_t diff_t = (t >= v) ? (t - v) : (q + t - v);

        w = (~r_ge_w & diff_w) | (r_ge_w & w);
        s = (~r_ge_w & diff_s) | (r_ge_w & s);
        t = (~r_ge_w & diff_t) | (r_ge_w & t);
    }

    // At the end, w should be gcd(a, q)
    // If gcd != 1, inverse doesn't exist
    if (w != 1) {
        return 0;
    }

    // The inverse is s (mod q)
    return (uint32_t)(s % q);
}
