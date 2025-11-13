#include "ntt64.h"
#include <string.h>

// ============================================================================
// MODULI AND PARAMETERS
// ============================================================================

// Prime moduli (all satisfy q ≡ 1 mod 128 for N=64 negacyclic NTT)
static const uint32_t Q[NTT_NUM_LAYERS] = {
    257u,        // 2^8 + 1
    3329u,       // Kyber-like
    12289u,      // 2^13 + 2^12 + 1
    40961u,      // 2^15 + 2^13 + 1
    65537u,      // 2^16 + 1 (Fermat prime)
    786433u,     // 2^19 + 2^18 + 1
    2013265921u  // 2^31 - 2^27 + 1
};

// Inverse of N=64 for each modulus (N_inv = 64^(-1) mod q)
static const uint32_t N_INV[NTT_NUM_LAYERS] = {
    253u,        // for q=257
    3277u,       // for q=3329
    12097u,      // for q=12289
    40321u,      // for q=40961
    64513u,      // for q=65537
    774145u,     // for q=786433
    1981808641u  // for q=2013265921
};

// Barrett reduction constants: floor(2^64 / q)
static const uint64_t BARRETT_CONST[NTT_NUM_LAYERS] = {
    71777214294589695ULL,    // for q=257
    5541226816974932ULL,     // for q=3329
    1501077717772768ULL,     // for q=12289
    450348967889200ULL,      // for q=40961
    281470681808895ULL,      // for q=65537
    23456218233097ULL,       // for q=786433
    9162596893ULL            // for q=2013265921
};

// ============================================================================
// PRECOMPUTED TWIDDLE FACTORS (for speed optimization)
// ============================================================================

// Precomputed twiddle factors for forward NTT
// twiddles_fwd[layer][stage] = omega^(64 / (2^(stage+1)))
static const uint32_t TWIDDLES_FWD[NTT_NUM_LAYERS][6] = {
    { 256u, 241u, 64u, 249u, 136u, 81u },         // Layer 0 (q=257)
    { 3328u, 1729u, 749u, 2699u, 2532u, 1996u },  // Layer 1 (q=3329)
    { 12288u, 1479u, 8246u, 4134u, 5860u, 7311u }, // Layer 2 (q=12289)
    { 40960u, 14541u, 31679u, 32406u, 19808u, 14529u }, // Layer 3 (q=40961)
    { 65536u, 65281u, 4096u, 64u, 65529u, 8224u },  // Layer 4 (q=65537)
    { 786432u, 100025u, 398184u, 570203u, 417470u, 362821u }, // Layer 5 (q=786433)
    { 2013265920u, 1728404513u, 1592366214u, 196396260u, 760005850u, 1721589904u } // Layer 6 (q=2013265921)
};

// Precomputed twiddle factors for inverse NTT
// twiddles_inv[layer][stage] = omega_inv^(64 / (2^(stage+1)))
static const uint32_t TWIDDLES_INV[NTT_NUM_LAYERS][6] = {
    { 256u, 16u, 253u, 32u, 240u, 165u },         // Layer 0 (q=257)
    { 3328u, 1600u, 3289u, 1897u, 2786u, 1426u }, // Layer 1 (q=3329)
    { 12288u, 10810u, 7143u, 10984u, 8747u, 9650u }, // Layer 2 (q=12289)
    { 40960u, 26420u, 3067u, 17816u, 20313u, 3572u }, // Layer 3 (q=40961)
    { 65536u, 256u, 65521u, 64513u, 8192u, 64509u },  // Layer 4 (q=65537)
    { 786432u, 686408u, 544685u, 541396u, 462518u, 596872u }, // Layer 5 (q=786433)
    { 2013265920u, 284861408u, 1801542727u, 567209306u, 1273220281u, 662200255u } // Layer 6 (q=2013265921)
};

// Precomputed psi powers for preprocessing: psi_powers[layer][i] = psi^i mod q
static const uint32_t PSI_POWERS[NTT_NUM_LAYERS][NTT_N] = {
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
    { // Layer 2 (q=12289)
        1u,12149u,7311u,8736u,5860u,2963u,3006u,9275u,4134u,11112u,5023u,9542u,3621u,9198u,2625u,1170u,
        8246u,726u,8961u,11227u,1212u,2366u,563u,7203u,11567u,2768u,5728u,9154u,8785u,11289u,4821u,955u,
        1479u,1853u,10938u,4805u,3195u,7393u,9545u,3201u,6553u,4255u,6461u,4846u,9744u,12208u,11340u,9970u,
        5146u,4611u,5777u,2294u,10643u,9238u,9314u,10963u,1305u,1635u,4591u,8577u,3542u,7969u,2639u,11499u
    },
    { // Layer 3 (q=40961)
        1u,19734u,14529u,29247u,19808u,249u,39407u,13153u,32406u,16872u,21040u,22664u,39178u,40738u,23106u,36913u,
        31679u,6604u,26395u,18854u,16073u,23559u,5956u,18595u,25092u,28960u,8768u,8448u,1562u,21836u,1904u,12299u,
        14541u,20289u,30312u,23525u,31337u,16141u,13758u,10864u,302u,20323u,4931u,25979u,1710u,34237u,22224u,39950u,
        37894u,16180u,5125u,4041u,34988u,14576u,14642u,6334u,23145u,28280u,24856u,329u,20648u,28565u,37389u,4033u
    },
    { // Layer 4 (q=65537)
        1u,13987u,8224u,11653u,65529u,19178u,65282u,37850u,64u,43187u,2040u,24885u,65025u,47726u,49217u,63068u,
        4096u,11414u,65023u,19752u,32769u,39762u,4112u,38595u,65533u,9589u,32641u,18925u,32u,54362u,1020u,45211u,
        65281u,23863u,57377u,31534u,2048u,5707u,65280u,9876u,49153u,19881u,2056u,52066u,65535u,37563u,49089u,42231u,
        16u,27181u,510u,55374u,65409u,44700u,61457u,15767u,1024u,35622u,32640u,4938u,57345u,42709u,1028u,26033u
    },
    { // Layer 5 (q=786433)
        1u,381732u,362821u,97476u,417470u,447786u,673503u,116568u,570203u,524454u,198384u,741786u,387372u,77747u,109250u,465443u,
        398184u,163747u,202098u,555735u,744837u,344431u,533387u,236852u,144953u,559149u,71971u,383350u,655292u,462836u,60605u,368299u,
        100025u,634717u,433307u,626999u,203749u,76001u,500362u,58542u,74616u,284518u,82144u,336832u,116823u,394171u,244715u,675341u,
        241748u,540017u,378618u,736069u,377103u,440344u,419955u,613608u,245037u,123064u,678026u,469969u,323915u,219489u,189561u,226456u
    },
    { // Layer 6 (q=2013265921)
        1u,397765732u,1721589904u,465591760u,760005850u,1480042919u,871747792u,1491681022u,196396260u,1553007845u,492152090u,1790034783u,1240658731u,1162339639u,939236864u,132263909u,
        1592366214u,1474561152u,1411014714u,1900691439u,177390144u,544217376u,328902512u,1700600734u,78945800u,603134811u,1606274035u,1964161403u,1399190761u,912838049u,918899846u,189150126u,
        1728404513u,1458586410u,1941224298u,717205599u,889310574u,632005205u,288289890u,1230352802u,1400279418u,7658004u,95586718u,23215881u,1561292356u,1547495552u,841453531u,1225971559u,
        211723194u,1091445674u,686375053u,418675046u,1424376889u,1229036302u,1333217202u,1274926174u,1446056615u,1192577619u,55559234u,1777795839u,740045640u,1238851905u,1351065666u,156720578u
    }
};

// Precomputed psi inverse powers for postprocessing: psi_inv_powers[layer][i] = psi_inv^i mod q
static const uint32_t PSI_INV_POWERS[NTT_NUM_LAYERS][NTT_N] = {
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
    { // Layer 2 (q=12289)
        1u,790u,9650u,4320u,8747u,3712u,7698u,10654u,10984u,1326u,2975u,3051u,1646u,9995u,6512u,7678u,
        7143u,2319u,949u,81u,2545u,7443u,5828u,8034u,5736u,9088u,2744u,4896u,9094u,7484u,1351u,10436u,
        10810u,11334u,7468u,1000u,3504u,3135u,6561u,9521u,722u,5086u,11726u,9923u,11077u,1062u,3328u,11563u,
        4043u,11119u,9664u,3091u,8668u,2747u,7266u,1177u,8155u,3014u,9283u,9326u,6429u,3553u,4978u,140u
    },
    { // Layer 3 (q=40961)
        1u,36928u,3572u,12396u,20313u,40632u,16105u,12681u,17816u,34627u,26319u,26385u,5973u,36920u,35836u,24781u,
        3067u,1011u,18737u,6724u,39251u,14982u,36030u,20638u,40659u,30097u,27203u,24820u,9624u,17436u,10649u,20672u,
        26420u,28662u,39057u,19125u,39399u,32513u,32193u,12001u,15869u,22366u,35005u,17402u,24888u,22107u,14566u,34357u,
        9282u,4048u,17855u,223u,1783u,18297u,19921u,24089u,8555u,27808u,1554u,40712u,21153u,11714u,26432u,21227u
    },
    { // Layer 4 (q=65537)
        1u,39504u,64509u,22828u,8192u,60599u,32897u,29915u,64513u,49770u,4080u,20837u,128u,10163u,65027u,38356u,
        65521u,23306u,16448u,27974u,2u,13471u,63481u,45656u,16384u,55661u,257u,59830u,63489u,34003u,8160u,41674u,
        256u,20326u,64517u,11175u,65505u,46612u,32896u,55948u,4u,26942u,61425u,25775u,32768u,45785u,514u,54123u,
        61441u,2469u,16320u,17811u,512u,40652u,63497u,22350u,65473u,27687u,255u,46359u,8u,53884u,57313u,51550u
    },
    { // Layer 5 (q=786433)
        1u,559977u,596872u,566944u,462518u,316464u,108407u,663369u,541396u,172825u,366478u,346089u,409330u,50364u,407815u,246416u,
        544685u,111092u,541718u,392262u,669610u,449601u,704289u,501915u,711817u,727891u,286071u,710432u,582684u,159434u,353126u,151716u,
        686408u,418134u,725828u,323597u,131141u,403083u,714462u,227284u,641480u,549581u,253046u,442002u,41596u,230698u,584335u,622686u,
        388249u,320990u,677183u,708686u,399061u,44647u,588049u,261979u,216230u,669865u,112930u,338647u,368963u,688957u,423612u,404701u
    },
    { // Layer 6 (q=2013265921)
        1u,1856545343u,662200255u,774414016u,1273220281u,235470082u,1957706687u,820688302u,567209306u,738339747u,680048719u,784229619u,588889032u,1594590875u,1326890868u,921820247u,
        1801542727u,787294362u,1171812390u,465770369u,451973565u,1990050040u,1917679203u,2005607917u,612986503u,782913119u,1724976031u,1381260716u,1123955347u,1296060322u,72041623u,554679511u,
        284861408u,1824115795u,1094366075u,1100427872u,614075160u,49104518u,406991886u,1410131110u,1934320121u,312665187u,1684363409u,1469048545u,1835875777u,112574482u,602251207u,538704769u,
        420899707u,1881002012u,1074029057u,850926282u,772607190u,223231138u,1521113831u,460258076u,1816869661u,521584899u,1141518129u,533223002u,1253260071u,1547674161u,291676017u,1615500189u
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
 */
static void bit_reverse_copy(uint32_t poly[NTT_N]) {
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
// FORWARD NTT
// ============================================================================

void ntt64_forward(uint32_t poly[NTT_N], int layer) {
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
// INVERSE NTT
// ============================================================================

void ntt64_inverse(uint32_t poly[NTT_N], int layer) {
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
// POINT-WISE MULTIPLICATION
// ============================================================================

void ntt64_pointwise_mul(uint32_t result[NTT_N],
                         const uint32_t a[NTT_N],
                         const uint32_t b[NTT_N],
                         int layer) {
    for (uint32_t i = 0; i < NTT_N; i++) {
        result[i] = ct_mul_mod(a[i], b[i], layer);
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint32_t ntt64_get_modulus(int layer) {
    return Q[layer];
}
