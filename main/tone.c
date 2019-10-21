#include "audio.h"
#include "tone.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Tone";

static TaskHandle_t s_tone_task_handle = NULL;

static int s_generate = 0;

// Samplerate: 44100, bitsPerSample: 16, time: 0.05 sec
// TODO: Proper tone generation (this is just a test)
int sine_size = 2205;
int16_t sine[] = {
	0, 2579, 5142, 7673, 10156, 12577, 14919, 17169, 
	19313, 21336, 23227, 24974, 26566, 27994, 29247, 30319, 
	31203, 31894, 32386, 32677, 32766, 32651, 32334, 31816, 
	31101, 30193, 29097, 27821, 26372, 24760, 22994, 21085, 
	19046, 16888, 14626, 12272, 9843, 7352, 4816, 2250, 
	-329, -2907, -5466, -7992, -10469, -12880, -15212, -17449, 
	-19578, -21585, -23458, -25186, -26758, -28163, -29394, -30443, 
	-31302, -31967, -32434, -32700, -32763, -32622, -32279, -31736, 
	-30996, -30064, -28945, -27646, -26176, -24543, -22758, -20832, 
	-18777, -16605, -14330, -11967, -9528, -7031, -4491, -1922, 
	658, 3234, 5791, 8311, 10780, 13182, 15502, 17726, 
	19841, 21831, 23687, 25395, 26946, 28330, 29538, 30563, 
	31398, 32038, 32479, 32719, 32756, 32590, 32221, 31653, 
	30888, 29931, 28789, 27468, 25977, 24324, 22520, 20577, 
	18506, 16321, 14034, 11660, 9213, 6710, 4164, 1593, 
	-987, -3562, -6114, -8629, -11090, -13483, -15792, -18002, 
	-20101, -22076, -23913, -25602, -27132, -28494, -29679, -30680, 
	-31490, -32105, -32521, -32735, -32746, -32554, -32160, -31566, 
	-30776, -29796, -28630, -27287, -25775, -24102, -22280, -20320, 
	-18234, -16034, -13736, -11351, -8897, -6387, -3838, -1264, 
	1316, 3889, 6437, 8946, 11400, 13782, 16079, 18276, 
	20360, 22318, 24137, 25806, 27315, 28655, 29817, 30794, 
	31580, 32170, 32560, 32748, 32733, 32515, 32095, 31476, 
	30662, 29657, 28469, 27103, 25570, 23878, 22038, 20061, 
	17959, 15747, 13436, 11042, 8580, 6064, 3511, 935, 
	-1644, -4215, -6760, -9262, -11708, -14080, -16365, -18549, 
	-20617, -22558, -24358, -26008, -27496, -28813, -29952, -30905, 
	-31666, -32230, -32595, -32757, -32717, -32473, -32027, -31383, 
	-30544, -29516, -28304, -26917, -25363, -23651, -21793, -19800, 
	-17683, -15457, -13135, -10732, -8261, -5740, -3183, -606, 
	1973, 4541, 7081, 9578, 12014, 14376, 16649, 18819, 
	20872, 22795, 24577, 26207, 27673, 28969, 30084, 31013, 
	31749, 32288, 32627, 32764, 32697, 32427, 31956, 31287, 
	30424, 29371, 28137, 26728, 25153, 23422, 21546, 19536, 
	17405, 15166, 12833, 10420, 7943, 5416, 2855, 277, 
	-2302, -4867, -7402, -9892, -12320, -14672, -16932, -19088, 
	-21125, -23031, -24794, -26403, -27848, -29121, -30213, -31117, 
	-31829, -32343, -32656, -32766, -32674, -32378, -31882, -31187, 
	-30300, -29224, -27967, -26536, -24941, -23191, -21297, -19271, 
	-17126, -14874, -12529, -10108, -7623, -5091, -2527, 51, 
	2630, 5192, 7723, 10205, 12624, 14965, 17213, 19354, 
	21375, 23264, 25008, 26597, 28020, 29270, 30339, 31219, 
	31905, 32394, 32681, 32766, 32647, 32326, 31804, 31085, 
	30173, 29074, 27794, 26342, 24726, 22957, 21046, 19004, 
	16844, 14580, 12225, 9794, 7302, 4766, 2199, -380, 
	-2958, -5517, -8042, -10517, -12927, -15257, -17492, -19619, 
	-21624, -23494, -25219, -26787, -28190, -29417, -30462, -31317, 
	-31979, -32442, -32703, -32762, -32617, -32270, -31723, -30979, 
	-30043, -28920, -27618, -26145, -24509, -22721, -20793, -18735, 
	-16561, -14284, -11919, -9479, -6981, -4440, -1871, 709, 
	3285, 5841, 8361, 10829, 13229, 15548, 17770, 19881, 
	21870, 23722, 25428, 26976, 28356, 29560, 30581, 31412, 
	32049, 32486, 32722, 32755, 32584, 32212, 31639, 30871, 
	29910, 28764, 27440, 25945, 24290, 22483, 20537, 18464, 
	16276, 13987, 11612, 9164, 6659, 4113, 1542, -1038, 
	-3613, -6165, -8679, -11139, -13530, -15837, -18045, -20142, 
	-22114, -23948, -25634, -27161, -28519, -29701, -30698, -31504, 
	-32116, -32527, -32738, -32744, -32548, -32150, -31552, -30759, 
	-29774, -28605, -27259, -25743, -24067, -22243, -20280, -18191, 
	-15990, -13689, -11303, -8847, -6337, -3787, -1213, 1367, 
	3940, 6488, 8996, 11448, 13829, 16124, 18319, 20401, 
	22355, 24172, 25838, 27344, 28680, 29838, 30811, 31593, 
	32179, 32566, 32750, 32731, 32509, 32085, 31462, 30644, 
	29635, 28443, 27075, 25538, 23843, 22000, 20020, 17916, 
	15702, 13389, 10994, 8530, 6013, 3459, 884, -1696, 
	-4266, -6810, -9312, -11755, -14126, -16410, -18591, -20657, 
	-22595, -24393, -26039, -27524, -28838, -29973, -30922, -31679, 
	-32240, -32600, -32759, -32714, -32466, -32016, -31368, -30526, 
	-29493, -28278, -26888, -25330, -23616, -21755, -19759, -17640, 
	-15412, -13088, -10683, -8212, -5690, -3132, -555, 2024, 
	4592, 7132, 9627, 12062, 14423, 16694, 18861, 20912, 
	22832, 24611, 26237, 27701, 28993, 30104, 31029, 31762, 
	32297, 32632, 32764, 32693, 32420, 31945, 31272, 30404, 
	29349, 28111, 26698, 25120, 23387, 21508, 19495, 17362, 
	15121, 12786, 10371, 7893, 5365, 2804, 226, -2353, 
	-4918, -7452, -9941, -12368, -14717, -16976, -19129, -21164, 
	-23067, -24827, -26433, -27875, -29144, -30233, -31133, -31841, 
	-32351, -32660, -32766, -32670, -32370, -31870, -31172, -30280, 
	-29201, -27940, -26506, -24908, -23155, -21258, -19230, -17082, 
	-14828, -12482, -10059, -7573, -5040, -2476, 102, 2681, 
	5243, 7773, 10254, 12672, 15011, 17257, 19396, 21414, 
	23300, 25041, 26626, 28047, 29293, 30358, 31234, 31917, 
	32401, 32685, 32765, 32643, 32317, 31792, 31069, 30153, 
	29050, 27767, 26311, 24693, 22921, 21007, 18962, 16800, 
	14534, 12177, 9745, 7252, 4715, 2148, -431, -3009, 
	-5568, -8092, -10566, -12975, -15303, -17536, -19660, -21662, 
	-23530, -25252, -26817, -28216, -29439, -30480, -31332, -31990, 
	-32449, -32706, -32761, -32612, -32262, -31710, -30963, -30023, 
	-28896, -27591, -26114, -24475, -22684, -20753, -18693, -16517, 
	-14238, -11871, -9430, -6931, -4389, -1819, 760, 3336, 
	5892, 8410, 10877, 13276, 15593, 17813, 19922, 21908, 
	23758, 25460, 27005, 28382, 29582, 30600, 31427, 32059, 
	32493, 32725, 32753, 32579, 32202, 31626, 30853, 29889, 
	28740, 27412, 25914, 24255, 22446, 20497, 18422, 16231, 
	13941, 11563, 9115, 6609, 4062, 1491, -1089, -3664, 
	-6215, -8728, -11187, -13576, -15882, -18088, -20182, -22152, 
	-23983, -25666, -27190, -28545, -29722, -30716, -31519, -32126, 
	-32534, -32740, -32742, -32542, -32140, -31538, -30741, -29753, 
	-28580, -27230, -25711, -24033, -22205, -20239, -18148, -15945, 
	-13642, -11255, -8798, -6286, -3736, -1162, 1418, 3991, 
	6538, 9045, 11496, 13875, 16169, 18362, 20441, 22393, 
	24206, 25869, 27372, 28705, 29859, 30829, 31607, 32189, 
	32571, 32751, 32728, 32502, 32074, 31447, 30625, 29613, 
	28418, 27046, 25506, 23808, 21962, 19980, 17873, 15656, 
	13342, 10945, 8480, 5963, 3408, 833, -1747, -4317, 
	-6860, -9361, -11803, -14173, -16454, -18633, -20697, -22632, 
	-24427, -26070, -27552, -28862, -29994, -30939, -31692, -32249, 
	-32605, -32760, -32711, -32459, -32005, -31353, -30507, -29471, 
	-28252, -26858, -25298, -23580, -21716, -19718, -17597, -15367, 
	-13041, -10635, -8162, -5639, -3081, -504, 2076, 4643, 
	7182, 9676, 12110, 14469, 16738, 18903, 20951, 22869, 
	24645, 26268, 27728, 29016, 30125, 31046, 31774, 32305, 
	32636, 32765, 32690, 32412, 31933, 31256, 30385, 29326, 
	28084, 26669, 25087, 23351, 21469, 19454, 17318, 15075, 
	12738, 10323, 7843, 5314, 2753, 175, -2404, -4969, 
	-7502, -9990, -12415, -14763, -17020, -19171, -21203, -23104, 
	-24861, -26464, -27902, -29168, -30252, -31149, -31853, -32359, 
	-32664, -32766, -32666, -32362, -31858, -31156, -30261, -29177, 
	-27913, -26476, -24874, -23118, -21219, -19188, -17038, -14782, 
	-12434, -10010, -7523, -4989, -2425, 154, 2732, 5294, 
	7823, 10303, 12719, 15056, 17300, 19437, 21453, 23336, 
	25074, 26656, 28073, 29316, 30377, 31250, 31928, 32409, 
	32688, 32765, 32638, 32309, 31779, 31052, 30133, 29026, 
	27740, 26281, 24659, 22884, 20967, 18920, 16756, 14488, 
	12129, 9696, 7202, 4664, 2097, -483, -3060, -5618, 
	-8142, -10615, -13022, -15348, -17579, -19701, -21701, -23566, 
	-25284, -26846, -28242, -29462, -30499, -31347, -32001, -32456, 
	-32709, -32760, -32607, -32253, -31697, -30946, -30002, -28872, 
	-27563, -26083, -24441, -22647, -20713, -18651, -16472, -14192, 
	-11823, -9381, -6881, -4338, -1768, 812, 3388, 5942, 
	8460, 10925, 13323, 15638, 17856, 19963, 21946, 23793, 
	25493, 27034, 28407, 29604, 30618, 31442, 32070, 32499, 
	32727, 32752, 32573, 32193, 31612, 30836, 29868, 28715, 
	27384, 25882, 24220, 22408, 20457, 18379, 16187, 13894, 
	11515, 9065, 6559, 4011, 1439, -1141, -3715, -6266, 
	-8778, -11235, -13623, -15926, -18131, -20223, -22189, -24018, 
	-25698, -27218, -28570, -29744, -30734, -31533, -32136, -32540, 
	-32742, -32740, -32536, -32130, -31524, -30723, -29731, -28555, 
	-27201, -25679, -23998, -22167, -20199, -18106, -15900, -13595, 
	-11207, -8748, -6236, -3685, -1110, 1470, 4042, 6588, 
	9094, 11544, 13922, 16213, 18404, 20481, 22430, 24241, 
	25901, 27400, 28730, 29881, 30846, 31620, 32198, 32577, 
	32753, 32726, 32496, 32064, 31433, 30607, 29591, 28392, 
	27017, 25473, 23772, 21924, 19939, 17830, 15611, 13295, 
	10897, 8431, 5912, 3357, 781, -1798, -4368, -6910, 
	-9410, -11851, -14219, -16498, -18676, -20737, -22669, -24461, 
	-26101, -27579, -28886, -30014, -30956, -31705, -32258, -32610, 
	-32761, -32708, -32452, -31994, -31338, -30488, -29449, -28226, 
	-26829, -25265, -23545, -21678, -19677, -17553, -15321, -12994, 
	-10586, -8112, -5588, -3030, -452, 2127, 4694, 7232, 
	9725, 12158, 14515, 16782, 18945, 20991, 22906, 24679, 
	26299, 27756, 29040, 30145, 31062, 31787, 32314, 32641, 
	32765, 32686, 32404, 31922, 31241, 30366, 29303, 28058, 
	26639, 25054, 23314, 21430, 19412, 17275, 15029, 12691, 
	10274, 7793, 5264, 2702, 123, -2455, -5019, -7552, 
	-10039, -12463, -14809, -17064, -19213, -21242, -23140, -24894, 
	-26494, -27929, -29191, -30272, -31165, -31865, -32367, -32668, 
	-32766, -32662, -32354, -31846, -31140, -30241, -29154, -27886, 
	-26446, -24841, -23082, -21180, -19146, -16994, -14736, -12387, 
	-9961, -7473, -4939, -2374, 205, 2783, 5344, 7872, 
	10351, 12766, 15102, 17344, 19478, 21492, 23372, 25107, 
	26686, 28100, 29339, 30397, 31265, 31940, 32417, 32692, 
	32764, 32634, 32300, 31767, 31036, 30113, 29002, 27712, 
	26250, 24625, 22847, 20928, 18878, 16712, 14441, 12082, 
	9647, 7152, 4613, 2045, -534, -3111, -5669, -8191, 
	-10663, -13069, -15393, -17622, -19742, -21739, -23601, -25317, 
	-26876, -28268, -29484, -30518, -31362, -32012, -32463, -32712, 
	-32759, -32602, -32243, -31684, -30929, -29981, -28848, -27535, 
	-26052, -24407, -22610, -20673, -18608, -16428, -14145, -11775, 
	-9332, -6831, -4287, -1717, 863, 3439, 5993, 8510, 
	10974, 13370, 15683, 17899, 20004, 21984, 23828, 25525, 
	27063, 28433, 29626, 30636, 31456, 32080, 32506, 32730, 
	32750, 32568, 32183, 31599, 30819, 29847, 28690, 27355, 
	25851, 24186, 22371, 20417, 18336, 16142, 13848, 11467, 
	9016, 6508, 3960, 1388, -1192, -3766, -6316, -8827, 
	-11283, -13670, -15971, -18174, -20263, -22227, -24053, -25730, 
	-27247, -28595, -29766, -30751, -31546, -32146, -32546, -32744, 
	-32738, -32530, -32120, -31510, -30705, -29710, -28530, -27173, 
	-25647, -23963, -22129, -20159, -18063, -15855, -13549, -11158, 
	-8699, -6185, -3634, -1059, 1521, 4092, 6639, 9144, 
	11592, 13968, 16258, 18447, 20521, 22468, 24275, 25932, 
	27428, 28754, 29902, 30864, 31634, 32208, 32582, 32754, 
	32723, 32489, 32053, 31418, 30589, 29569, 28366, 26988, 
	25441, 23737, 21885, 19898, 17787, 15566, 13248, 10848, 
	8381, 5862, 3306, 730, -1850, -4419, -6961, -9459, 
	-11899, -14265, -16543, -18718, -20776, -22706, -24495, -26132, 
	-27607, -28911, -30035, -30972, -31718, -32267, -32615, -32762, 
	-32705, -32445, -31983, -31323, -30469, -29426, -28200, -26799, 
	-25232, -23509, -21639, -19636, -17510, -15276, -12947, -10537, 
	-8063, -5538, -2979, -401, 2178, 4745, 7282, 9774, 
	12205, 14561, 16826, 18987, 21030, 22942, 24713, 26329, 
	27783, 29064, 30165, 31078, 31799, 32322, 32645, 32766, 
	32683, 32397, 31910, 31225, 30347, 29280, 28031, 26609, 
	25021, 23278, 21391, 19371, 17231, 14984, 12644, 10225, 
	7743, 5213, 2651, 72, -2506, -5070, -7602, -10088, 
	-12510, -14855, -17108, -19254, -21281, -23176, -24928, -26524, 
	-27956, -29215, -30292, -31181, -31877, -32375, -32672, -32766, 
	-32657, -32346, -31834, -31124, -30221, -29131, -27859, -26415, 
	-24807, -23046, -21141, -19105, -16950, -14690, -12339, -9912, 
	-7423, -4888, -2323, 256, 2834, 5395, 7922, 10400, 
	12814, 15148, 17387, 19520, 21530, 23408, 25140, 26716, 
	28126, 29362, 30416, 31281, 31951, 32424, 32695, 32764, 
	32629, 32292, 31754, 31019, 30092, 28978, 27685, 26219, 
	24591, 22810, 20888, 18836, 16668, 14395, 12034, 9598, 
	7102, 4562, 1994, -585, -3162, -5719, -8241, -10712, 
	-13116, -15439, -17666, -19783, -21777, -23637, -25350, -26905, 
	-28294, -29507, -30537, -31377, -32023, -32470, -32715, -32758, 
	-32597, -32234, -31671, -30912, -29961, -28823, -27507, -26021, 
	-24372, -22573, -20634, -18566, -16383, -14099, -11727, -9283, 
	-6780, -4236, -1665, 914, 3490, 6043, 8559, 11022, 
	13417, 15728, 17942, 20044, 22022, 23864, 25557, 27092, 
	28458, 29648, 30654, 31470, 32091, 32512, 32732, 32749, 
	32562, 32174, 31585, 30801, 29826, 28665, 27327, 25819, 
	24151, 22333, 20377, 18294, 16097, 13801, 11419, 8966, 
	6458, 3909, 1337, -1243, -3817, -6366, -8877, -11332, 
	-13716, -16016, -18216, -20304, -22265, -24088, -25762, -27275, 
	-28620, -29787, -30769, -31560, -32156, -32552, -32746, -32736, 
	-32524, -32110, -31496, -30687, -29688, -28504, -27144, -25615, 
	-23928, -22091, -20118, -18020, -15810, -13502, -11110, -8649, 
	-6135, -3582, -1008, 1572, 4143, 6689, 9193, 11640, 
	14015, 16302, 18489, 20561, 22505, 24310, 25964, 27456, 
	28779, 29923, 30881, 31647, 32217, 32588, 32756, 32720, 
	32482, 32042, 31404, 30570, 29547, 28341, 26958, 25409, 
	23701, 21847, 19857, 17744, 15521, 13201, 10800, 8332, 
	5811, 3255, 679, -1901, -4470, -7011, -9508, -11947, 
	-14311, -16587, -18760, -20816, -22743, -24529, -26163, -27635, 
	-28935, -30055, -30989, -31731, -32276, -32620, -32762, -32701, 
	-32437, -31972, -31308, -30450, -29403, -28174, -26770, -25200, 
	-23473, -21601, -19594, -17467, -15230, -12899, -10489, -8013, 
	-5487, -2927, -350, 2229, 4796, 7332, 9823, 12253, 
	14607, 16870, 19029, 21069, 22979, 24746, 26360, 27810, 
	29088, 30185, 31095, 31811, 32331, 32650, 32766, 32679, 
	32389, 31898, 31210, 30327, 29257, 28005, 26579, 24988, 
	23242, 21352, 19330, 17187, 14938, 12596, 10176, 7693, 
	5162, 2600, 21, -2558, -5121, -7652, -10136, -12557, 
	-14901, -17151, -19296, -21320, -23213, -24961, -26554, -27983, 
	-29238, -30311, -31197, -31889, -32383, -32676, -32766, -32653, 
	-32338, -31821, -31108, -30201, -29107, -27832, -26385, -24774, 
	-23009, -21101, -19063, -16906, -14644, -12292, -9863, -7373, 
	-4837, -2271, 308, 2886, 5446, 7972, 10449, 12861, 
	15193, 17431, 19561, 21569, 23444, 25173, 26746, 28153, 
	29385, 30435, 31296, 31963, 32431, 32699, 32763, 32624, 
	32283, 31741, 31003, 30072, 28954, 27657, 26188, 24557, 
	22774, 20849, 18794, 16623, 14349, 11986, 9549, 7052, 
	4511, 1943, -637, -3213, -5770, -8291, -10760, -13163, 
	-15484, -17709, -19824, -21816, -23672, -25382, -26934, -28320, 
	-29529, -30555, -31392, -32034, -32477, -32718, -32757, -32592, 
	-32225, -31658, -30895, -29940, -28799, -27479, -25989, -24338, 
	-22536, -20594, -18524, -16339, -14053, -11679, -9233, -6730, 
	-4185, -1614, 966, 3541, 6094, 8609, 11071, 13464, 
	15773, 17985, 20085, 22060, 23899, 25589, 27121, 28484, 
	29670, 30673, 31485, 32101, 32519, 32734, 32747, 32556, 
	32164, 31572, 30784, 29804, 28640, 27299, 25788, 24116, 
	22296, 20337, 18251, 16053, 13755, 11371, 8917, 6408, 
	3858, 1285, -1295, -3868, -6417, -8926, -11380, -13763, 
	-16061, -18259, -20344, -22303, -24123, -25793, -27304, -28645, 
	-29808, -30787, -31574, -32166, -32557, -32747, -32734, -32518, 
	-32099, -31482, -30669, -29666, -28479, -27115, -25583, -23892, 
	-22053, -20077, -17977, -15765, -13455, -11062, -8600, -6085, 
	-3531, -956, 1624, 4194, 6739, 9242, 11688, 14061, 
	16347, 18531, 20601, 22543, 24344, 25995, 27484, 28803, 
	29943, 30898, 31660, 32227, 32593, 32757, 32718, 32475, 
	32032, 31389, 30552, 29525, 28315, 26929, 25376, 23666, 
	21809, 19816, 17701, 15476, 13154, 10751, 8282, 5761, 
	3204, 627, -1952, -4521, -7061, -9558, -11995, -14358, 
	-16631, -18802, -20856, -22780, -24563, -26194, -27662, -28959, 
	-30076, -31006, -31744, -32285, -32625, -32763, -32698, -32430, 
	-31961, -31293, -30431, -29381, -28148, -26740, -25167, -23437, 
	-21562, -19553, -17423, -15185, -12852, -10440, -7963, -5437, 
	-2876, -298, 2281, 4846, 7382, 9872, 12300, 14653, 
	16914, 19071, 21109, 23016, 24780, 26390, 27837, 29111, 
	30205, 31111, 31824, 32339, 32654, 32766, 32675, 32381, 
	31887, 31194, 30308, 29234, 27978, 26549, 24955, 23206, 
	21313, 19288, 17143, 14892, 12549, 10128, 7643, 5112, 
	2548, -30, -2609, -5172, -7702, -10185, -12605, -14946, 
	-17195, -19337, -21359, -23249, -24994
};


static void tone_task(void *arg) {
    struct audio_state *state = (struct audio_state *) arg;

    int16_t data[8192] = {0};
    int data_len = 8192;

    while (state->running) {
        if (state->buffer_assigned[SOURCE_TONE] == -1) {
            ESP_LOGD(TAG, "No buffer assigned");
            vTaskDelay(1000/portTICK_PERIOD_MS);
            continue;
        }

        if (s_generate) {
            /* audio_write_ringbuf((void*)sine, sine_size, SOURCE_TONE); */
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < sine_size; j++) {
                    data[i*sine_size + j] = sine[j];
                }
            }
            audio_write_ringbuf((void*)data, data_len, SOURCE_TONE);
            
            s_generate = 0;
        } else {
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }
}

void tone_task_start(struct audio_state *state) {
    xTaskCreate(tone_task, "Tone", 3*8192, state, configMAX_PRIORITIES-4, &s_tone_task_handle);
}

// TODO: Frequency control
void tone_gen() {
    s_generate = 1;
}