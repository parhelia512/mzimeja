// keychar.cpp
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"
#include "vksub.h"

#define UNBOOST_USE_STRING_ALGORITHM
#include "unboost.hpp"

//////////////////////////////////////////////////////////////////////////////

struct TABLEENTRY {
  LPCWSTR key;
  LPCWSTR value;
  LPCWSTR extra;
};

static TABLEENTRY halfkana_table[] = {
// 【降順にソート】ここから
  {L"ー", L"-"},
  {L"・", L"･"},
  {L"゜", L"ﾟ"},
  {L"゛", L"ﾞ"},
  {L"ん", L"ﾝ"},
  {L"を", L"ｦ"},
  {L"ゑ", L"ｴ"},
  {L"ゐ", L"ｲ"},
  {L"わ", L"ﾜ"},
  {L"ゎ", L"ﾜ"},
  {L"ろ", L"ﾛ"},
  {L"れ", L"ﾚ"},
  {L"る", L"ﾙ"},
  {L"り", L"ﾘ"},
  {L"ら", L"ﾗ"},
  {L"よ", L"ﾖ"},
  {L"ょ", L"ｮ"},
  {L"ゆ", L"ﾕ"},
  {L"ゅ", L"ｭ"},
  {L"や", L"ﾔ"},
  {L"ゃ", L"ｬ"},
  {L"も", L"ﾓ"},
  {L"め", L"ﾒ"},
  {L"む", L"ﾑ"},
  {L"み", L"ﾐ"},
  {L"ま", L"ﾏ"},
  {L"ぽ", L"ﾎﾟ"},
  {L"ぼ", L"ﾎﾞ"},
  {L"ほ", L"ﾎ"},
  {L"ぺ", L"ﾍﾟ"},
  {L"べ", L"ﾍﾞ"},
  {L"へ", L"ﾍ"},
  {L"ぷ", L"ﾌﾟ"},
  {L"ぶ", L"ﾌﾞ"},
  {L"ふ", L"ﾌ"},
  {L"ぴ", L"ﾋﾟ"},
  {L"び", L"ﾋﾞ"},
  {L"ひ", L"ﾋ"},
  {L"ぱ", L"ﾊﾟ"},
  {L"ば", L"ﾊﾞ"},
  {L"は", L"ﾊ"},
  {L"の", L"ﾉ"},
  {L"ね", L"ﾈ"},
  {L"ぬ", L"ﾇ"},
  {L"に", L"ﾆ"},
  {L"な", L"ﾅ"},
  {L"ど", L"ﾄﾞ"},
  {L"と", L"ﾄ"},
  {L"で", L"ﾃﾞ"},
  {L"て", L"ﾃ"},
  {L"づ", L"ﾂﾞ"},
  {L"つ", L"ﾂ"},
  {L"っ", L"ｯ"},
  {L"ぢ", L"ﾁﾞ"},
  {L"ち", L"ﾁ"},
  {L"だ", L"ﾀﾞ"},
  {L"た", L"ﾀ"},
  {L"ぞ", L"ｿﾞ"},
  {L"そ", L"ｿ"},
  {L"ぜ", L"ｾﾞ"},
  {L"せ", L"ｾ"},
  {L"ず", L"ｽﾞ"},
  {L"す", L"ｽ"},
  {L"じ", L"ｼﾞ"},
  {L"し", L"ｼ"},
  {L"ざ", L"ｻﾞ"},
  {L"さ", L"ｻ"},
  {L"ご", L"ｺﾞ"},
  {L"こ", L"ｺ"},
  {L"げ", L"ｹﾞ"},
  {L"け", L"ｹ"},
  {L"ぐ", L"ｸﾞ"},
  {L"く", L"ｸ"},
  {L"ぎ", L"ｷﾞ"},
  {L"き", L"ｷ"},
  {L"が", L"ｶﾞ"},
  {L"か", L"ｶ"},
  {L"お", L"ｵ"},
  {L"ぉ", L"ｫ"},
  {L"え", L"ｴ"},
  {L"ぇ", L"ｪ"},
  {L"う゛", L"ｳﾞ"},
  {L"う", L"ｳ"},
  {L"ぅ", L"ｩ"},
  {L"い", L"ｲ"},
  {L"ぃ", L"ｨ"},
  {L"あ", L"ｱ"},
  {L"ぁ", L"ｧ"},
  {L"」", L"｣"},
  {L"「", L"｢"},
  {L"。", L"｡"},
  {L"、", L"､"},
// 【降順にソート】ここまで
};

static TABLEENTRY kana_table[] = {
// 【降順にソート】ここから
  {L"ﾟ", L"゜"},
  {L"ﾞ", L"゛"},
  {L"ﾝ", L"ん"},
  {L"ﾜ", L"わ"},
  {L"ﾛ", L"ろ"},
  {L"ﾚ", L"れ"},
  {L"ﾙ", L"る"},
  {L"ﾘ", L"り"},
  {L"ﾗ", L"ら"},
  {L"ﾖ", L"よ"},
  {L"ﾕ", L"ゆ"},
  {L"ﾔ", L"や"},
  {L"ﾓ", L"も"},
  {L"ﾒ", L"め"},
  {L"ﾑ", L"む"},
  {L"ﾐ", L"み"},
  {L"ﾏ", L"ま"},
  {L"ﾎ", L"ほ"},
  {L"ﾍ", L"へ"},
  {L"ﾌ", L"ふ"},
  {L"ﾋ", L"ひ"},
  {L"ﾊ", L"は"},
  {L"ﾉ", L"の"},
  {L"ﾈ", L"ね"},
  {L"ﾇ", L"ぬ"},
  {L"ﾆ", L"に"},
  {L"ﾅ", L"な"},
  {L"ﾄ", L"と"},
  {L"ﾃ", L"て"},
  {L"ﾂ", L"つ"},
  {L"ﾁ", L"ち"},
  {L"ﾀ", L"た"},
  {L"ｿ", L"そ"},
  {L"ｾ", L"せ"},
  {L"ｽ", L"す"},
  {L"ｼ", L"し"},
  {L"ｻ", L"さ"},
  {L"ｺ", L"こ"},
  {L"ｹ", L"け"},
  {L"ｸ", L"く"},
  {L"ｷ", L"き"},
  {L"ｶ", L"か"},
  {L"ｵ", L"お"},
  {L"ｴ", L"え"},
  {L"ｳ", L"う"},
  {L"ｲ", L"い"},
  {L"ｱ", L"あ"},
  {L"ｰ", L"ー"},
  {L"ｯ", L"っ"},
  {L"ｮ", L"ょ"},
  {L"ｭ", L"ゅ"},
  {L"ｬ", L"ゃ"},
  {L"ｫ", L"ぉ"},
  {L"ｪ", L"ぇ"},
  {L"ｩ", L"ぅ"},
  {L"ｨ", L"ぃ"},
  {L"ｧ", L"ぁ"},
  {L"ｦ", L"を"},
  {L"･", L"・"},
  {L"､", L"、"},
  {L"｣", L"」"},
  {L"｢", L"「"},
  {L"｡", L"。"},
  {L"ほﾟ", L"ぽ"},
  {L"ほﾞ", L"ぼ"},
  {L"へﾟ", L"ぺ"},
  {L"へﾞ", L"べ"},
  {L"ふﾟ", L"ぷ"},
  {L"ふﾞ", L"ぶ"},
  {L"ひﾟ", L"ぴ"},
  {L"ひﾞ", L"び"},
  {L"はﾟ", L"ぱ"},
  {L"はﾞ", L"ば"},
  {L"とﾞ", L"ど"},
  {L"てﾞ", L"で"},
  {L"つﾞ", L"づ"},
  {L"ちﾞ", L"ぢ"},
  {L"たﾞ", L"だ"},
  {L"そﾞ", L"ぞ"},
  {L"せﾞ", L"ぜ"},
  {L"すﾞ", L"ず"},
  {L"しﾞ", L"じ"},
  {L"さﾞ", L"ざ"},
  {L"こﾞ", L"ご"},
  {L"けﾞ", L"げ"},
  {L"くﾞ", L"ぐ"},
  {L"きﾞ", L"ぎ"},
  {L"かﾞ", L"が"},
  {L"うﾞ", L"ヴ"},
// 【降順にソート】ここまで
};

static TABLEENTRY kigou_table[] = {
// 【降順にソート】ここから
  {L"~", L"〜"},
  {L"}", L"｝"},
  {L"|", L"｜"},
  {L"{", L"｛"},
  {L"`", L"｀"},
  {L"_", L"＿"},
  {L"^", L"＾"},
  {L"]", L"」"},
  {L"\\", L"￥"},
  {L"[", L"「"},
  {L"@", L"＠"},
  {L"?", L"？"},
  {L">", L"＞"},
  {L"=", L"＝"},
  {L"<", L"＜"},
  {L";", L"；"},
  {L":", L"："},
  {L"/", L"・"},
  {L".", L"。"},
  {L"-", L"ー"},
  {L",", L"、"},
  {L"+", L"＋"},
  {L"*", L"＊"},
  {L")", L"）"},
  {L"(", L"（"},
  {L"'", L"’"},
  {L"&", L"＆"},
  {L"%", L"％"},
  {L"$", L"＄"},
  {L"#", L"＃"},
  {L"\"", L"″"},
  {L"!", L"！"},
// 【降順にソート】ここまで
};

static TABLEENTRY reverse_table[] = {
// 【降順にソート】ここから
  {L"〜", L"~"},
  {L"ー", L"-"},
  {L"んお", L"n'o"},
  {L"んえ", L"n'e"},
  {L"んう", L"n'u"},
  {L"んい", L"n'i"},
  {L"んあ", L"n'a"},
  {L"ん", L"n"},
  {L"を", L"wo"},
  {L"ゑ", L"we"},
  {L"ゐ", L"wi"},
  {L"わ", L"wa"},
  {L"ゎ", L"xwa"},
  {L"ろ", L"ro"},
  {L"れ", L"re"},
  {L"る", L"ru"},
  {L"りょ", L"ryo"},
  {L"りゅ", L"ryu"},
  {L"りゃ", L"rya"},
  {L"りぇ", L"rye"},
  {L"りぃ", L"ryi"},
  {L"り", L"ri"},
  {L"ら", L"ra"},
  {L"よ", L"yo"},
  {L"ょ", L"xyo"},
  {L"ゆ", L"yu"},
  {L"ゅ", L"xyu"},
  {L"や", L"ya"},
  {L"ゃ", L"xya"},
  {L"も", L"mo"},
  {L"め", L"me"},
  {L"む", L"mu"},
  {L"みょ", L"myo"},
  {L"みゅ", L"myu"},
  {L"みゃ", L"mya"},
  {L"みぇ", L"mye"},
  {L"みぃ", L"myi"},
  {L"み", L"mi"},
  {L"ま", L"ma"},
  {L"ぽ", L"po"},
  {L"ぼ", L"bo"},
  {L"ほ", L"ho"},
  {L"ぺ", L"pe"},
  {L"べ", L"be"},
  {L"へ", L"he"},
  {L"ぷ", L"pu"},
  {L"ぶ", L"bu"},
  {L"ふ", L"hu"},
  {L"ぴょ", L"pyo"},
  {L"ぴゅ", L"pyu"},
  {L"ぴゃ", L"pya"},
  {L"ぴぇ", L"pye"},
  {L"ぴぃ", L"pyi"},
  {L"ぴ", L"pi"},
  {L"びょ", L"byo"},
  {L"びゅ", L"byu"},
  {L"びゃ", L"bya"},
  {L"びぇ", L"bye"},
  {L"びぃ", L"byi"},
  {L"び", L"bi"},
  {L"ひょ", L"hyo"},
  {L"ひゅ", L"hyu"},
  {L"ひゃ", L"hya"},
  {L"ひぇ", L"hye"},
  {L"ひぃ", L"hyi"},
  {L"ひ", L"hi"},
  {L"ぱ", L"pa"},
  {L"ば", L"ba"},
  {L"は", L"ha"},
  {L"の", L"no"},
  {L"ね", L"ne"},
  {L"ぬ", L"nu"},
  {L"にょ", L"nyo"},
  {L"にゅ", L"nyu"},
  {L"にゃ", L"nya"},
  {L"にぇ", L"nye"},
  {L"にぃ", L"nyi"},
  {L"に", L"ni"},
  {L"な", L"na"},
  {L"ど", L"do"},
  {L"と", L"to"},
  {L"でぃ", L"dhi"},
  {L"で", L"de"},
  {L"て", L"te"},
  {L"づ", L"du"},
  {L"つ", L"tu"},
  {L"っを", L"wwo"},
  {L"っゑ", L"wwe"},
  {L"っゐ", L"wwi"},
  {L"っわ", L"wwa"},
  {L"っゎ", L"xxwa"},
  {L"っろ", L"rro"},
  {L"っれ", L"rre"},
  {L"っる", L"rru"},
  {L"っり", L"r", L"り"},
  {L"っら", L"rra"},
  {L"っよ", L"yyo"},
  {L"っょ", L"xxyo"},
  {L"っゆ", L"yyu"},
  {L"っゅ", L"xxyu"},
  {L"っや", L"yya"},
  {L"っゃ", L"xxya"},
  {L"っも", L"mmo"},
  {L"っめ", L"mme"},
  {L"っむ", L"mmu"},
  {L"っみ", L"m", L"み"},
  {L"っま", L"mma"},
  {L"っぽ", L"ppo"},
  {L"っぼ", L"bbo"},
  {L"っほ", L"hho"},
  {L"っぺ", L"ppe"},
  {L"っべ", L"bbe"},
  {L"っへ", L"hhe"},
  {L"っぷ", L"ppu"},
  {L"っぶ", L"bbu"},
  {L"っふ", L"hhu"},
  {L"っぴ", L"p", L"ぴ"},
  {L"っび", L"b", L"び"},
  {L"っひ", L"h", L"ひ"},
  {L"っぱ", L"ppa"},
  {L"っば", L"bba"},
  {L"っは", L"hha"},
  {L"っど", L"ddo"},
  {L"っと", L"tto"},
  {L"っで", L"dde"},
  {L"って", L"tte"},
  {L"っづ", L"ddu"},
  {L"っつ", L"ttu"},
  {L"っぢ", L"d", L"ぢ"},
  {L"っち", L"t", L"ち"},
  {L"っだ", L"dda"},
  {L"った", L"tta"},
  {L"っぞ", L"zzo"},
  {L"っそ", L"sso"},
  {L"っぜ", L"zze"},
  {L"っせ", L"sse"},
  {L"っず", L"zzu"},
  {L"っす", L"ssu"},
  {L"っじ", L"z", L"じ"},
  {L"っし", L"s", L"し"},
  {L"っざ", L"zza"},
  {L"っさ", L"ssa"},
  {L"っご", L"ggo"},
  {L"っこ", L"kko"},
  {L"っげ", L"gge"},
  {L"っけ", L"kke"},
  {L"っぐ", L"ggu"},
  {L"っく", L"kku"},
  {L"っぎ", L"g", L"ぎ"},
  {L"っき", L"k", L"き"},
  {L"っが", L"gga"},
  {L"っか", L"kka"},
  {L"っぉ", L"xxo"},
  {L"っぇ", L"xxe"},
  {L"っう゛", L"v", L"う゛"},
  {L"っぅ", L"xxu"},
  {L"っぃ", L"xxi"},
  {L"っぁ", L"xxa"},
  {L"っ", L"xtu"},
  {L"ぢょ", L"dyo"},
  {L"ぢゅ", L"dyu"},
  {L"ぢゃ", L"dya"},
  {L"ぢぇ", L"dye"},
  {L"ぢぃ", L"dyi"},
  {L"ぢ", L"di"},
  {L"ちょ", L"tyo"},
  {L"ちゅ", L"tyu"},
  {L"ちゃ", L"tya"},
  {L"ちぇ", L"tye"},
  {L"ちぃ", L"tyi"},
  {L"ち", L"ti"},
  {L"だ", L"da"},
  {L"た", L"ta"},
  {L"ぞ", L"zo"},
  {L"そ", L"so"},
  {L"ぜ", L"ze"},
  {L"せ", L"se"},
  {L"ず", L"zu"},
  {L"す", L"su"},
  {L"じょ", L"zyo"},
  {L"じゅ", L"zyu"},
  {L"じゃ", L"zya"},
  {L"じぇ", L"zye"},
  {L"じぃ", L"zyi"},
  {L"じ", L"zi"},
  {L"しょ", L"syo"},
  {L"しゅ", L"syu"},
  {L"しゃ", L"sya"},
  {L"しぇ", L"sye"},
  {L"しぃ", L"syi"},
  {L"し", L"si"},
  {L"ざ", L"za"},
  {L"さ", L"sa"},
  {L"ご", L"go"},
  {L"こ", L"ko"},
  {L"げ", L"ge"},
  {L"け", L"ke"},
  {L"ぐ", L"gu"},
  {L"く", L"ku"},
  {L"ぎょ", L"gyo"},
  {L"ぎゅ", L"gyu"},
  {L"ぎゃ", L"gya"},
  {L"ぎぇ", L"gye"},
  {L"ぎぃ", L"gyi"},
  {L"ぎ", L"gi"},
  {L"きょ", L"kyo"},
  {L"きゅ", L"kyu"},
  {L"きゃ", L"kya"},
  {L"きぇ", L"kye"},
  {L"きぃ", L"kyi"},
  {L"き", L"ki"},
  {L"が", L"ga"},
  {L"か", L"ka"},
  {L"お", L"o"},
  {L"ぉ", L"xo"},
  {L"え", L"e"},
  {L"ぇ", L"xe"},
  {L"う゛ぉ", L"vo"},
  {L"う゛ぇ", L"ve"},
  {L"う゛ぃ", L"vi"},
  {L"う゛ぁ", L"va"},
  {L"う゛", L"vu"},
  {L"う", L"u"},
  {L"ぅ", L"xu"},
  {L"い", L"i"},
  {L"ぃ", L"xi"},
  {L"あ", L"a"},
  {L"ぁ", L"xa"},
  {L"」", L"]"},
  {L"「", L"["},
// 【降順にソート】ここまで
};

static TABLEENTRY romaji_table[] = {
// 【降順にソート】ここから
  {L"zyu", L"じゅ"},
  {L"zyo", L"じょ"},
  {L"zyi", L"じぃ"},
  {L"zye", L"じぇ"},
  {L"zya", L"じゃ"},
  {L"zu", L"ず"},
  {L"zo", L"ぞ"},
  {L"zi", L"じ"},
  {L"ze", L"ぜ"},
  {L"za", L"ざ"},
  {L"yu", L"ゆ"},
  {L"yo", L"よ"},
  {L"yi", L"い"},
  {L"ye", L"いぇ"},
  {L"ya", L"や"},
  {L"xyu", L"ゅ"},
  {L"xyo", L"ょ"},
  {L"xyi", L"ぃ"},
  {L"xye", L"ぇ"},
  {L"xya", L"ゃ"},
  {L"xwa", L"ゎ"},
  {L"xu", L"ぅ"},
  {L"xtu", L"っ"},
  {L"xo", L"ぉ"},
  {L"xn", L"ん"},
  {L"xke", L"ヶ"},
  {L"xka", L"ヵ"},
  {L"xi", L"ぃ"},
  {L"xe", L"ぇ"},
  {L"xa", L"ぁ"},
  {L"wu", L"う"},
  {L"wo", L"を"},
  {L"wi", L"うぃ"},
  {L"whu", L"う"},
  {L"who", L"うぉ"},
  {L"whi", L"うぃ"},
  {L"whe", L"うぇ"},
  {L"wha", L"うぁ"},
  {L"we", L"うぇ"},
  {L"wa", L"わ"},
  {L"vyu", L"ヴゅ"},
  {L"vyo", L"ヴょ"},
  {L"vyi", L"ヴぃ"},
  {L"vye", L"ヴぇ"},
  {L"vya", L"ヴゃ"},
  {L"vu", L"ヴ"},
  {L"vo", L"ヴぉ"},
  {L"vi", L"ヴぃ"},
  {L"ve", L"ヴぇ"},
  {L"va", L"ヴぁ"},
  {L"u", L"う"},
  {L"tyu", L"ちゅ"},
  {L"tyo", L"ちょ"},
  {L"tyi", L"ちぃ"},
  {L"tye", L"ちぇ"},
  {L"tya", L"ちゃ"},
  {L"twu", L"とぅ"},
  {L"two", L"とぉ"},
  {L"twi", L"とぃ"},
  {L"twe", L"とぇ"},
  {L"twa", L"とぁ"},
  {L"tu", L"つ"},
  {L"tsu", L"つ"},
  {L"tso", L"つぉ"},
  {L"tsi", L"つぃ"},
  {L"tse", L"つぇ"},
  {L"tsa", L"つぁ"},
  {L"to", L"と"},
  {L"ti", L"ち"},
  {L"thu", L"てゅ"},
  {L"tho", L"てょ"},
  {L"thi", L"てぃ"},
  {L"the", L"てぇ"},
  {L"tha", L"てゃ"},
  {L"te", L"て"},
  {L"ta", L"た"},
  {L"syu", L"しゅ"},
  {L"syo", L"しょ"},
  {L"syi", L"しぃ"},
  {L"sye", L"しぇ"},
  {L"sya", L"しゃ"},
  {L"swu", L"すぅ"},
  {L"swo", L"すぉ"},
  {L"swi", L"すぃ"},
  {L"swe", L"すぇ"},
  {L"swa", L"すぁ"},
  {L"su", L"す"},
  {L"so", L"そ"},
  {L"si", L"し"},
  {L"shu", L"しゅ"},
  {L"sho", L"しょ"},
  {L"shi", L"し"},
  {L"she", L"しぇ"},
  {L"sha", L"しゃ"},
  {L"se", L"せ"},
  {L"sa", L"さ"},
  {L"ryu", L"りゅ"},
  {L"ryo", L"りょ"},
  {L"ryi", L"りぃ"},
  {L"rye", L"りぇ"},
  {L"rya", L"りゃ"},
  {L"ru", L"る"},
  {L"ro", L"ろ"},
  {L"ri", L"り"},
  {L"re", L"れ"},
  {L"ra", L"ら"},
  {L"qyu", L"くゅ"},
  {L"qyo", L"くょ"},
  {L"qyi", L"くぃ"},
  {L"qye", L"くぇ"},
  {L"qya", L"くゃ"},
  {L"qwu", L"くぅ"},
  {L"qwo", L"くぉ"},
  {L"qwi", L"くぃ"},
  {L"qwe", L"くぇ"},
  {L"qwa", L"くぁ"},
  {L"qu", L"く"},
  {L"qo", L"くぉ"},
  {L"qi", L"くぃ"},
  {L"qe", L"くぇ"},
  {L"qa", L"くぁ"},
  {L"pyu", L"ぴゅ"},
  {L"pyo", L"ぴょ"},
  {L"pyi", L"ぴぃ"},
  {L"pye", L"ぴぇ"},
  {L"pya", L"ぴゃ"},
  {L"pu", L"ぷ"},
  {L"po", L"ぽ"},
  {L"pi", L"ぴ"},
  {L"pe", L"ぺ"},
  {L"pa", L"ぱ"},
  {L"o", L"お"},
  {L"n’", L"ん"},
  {L"nz", L"ん", L"z"},
  {L"nyu", L"にゅ"},
  {L"nyo", L"にょ"},
  {L"nyi", L"にぃ"},
  {L"nye", L"にぇ"},
  {L"nya", L"にゃ"},
  {L"nx", L"ん", L"x"},
  {L"nw", L"ん", L"w"},
  {L"nv", L"ん", L"v"},
  {L"nu", L"ぬ"},
  {L"nt", L"ん", L"t"},
  {L"ns", L"ん", L"s"},
  {L"nr", L"ん", L"r"},
  {L"nq", L"ん", L"q"},
  {L"np", L"ん", L"p"},
  {L"no", L"の"},
  {L"nn", L"ん"},
  {L"nm", L"ん", L"m"},
  {L"nl", L"ん", L"l"},
  {L"nk", L"ん", L"k"},
  {L"nj", L"ん", L"j"},
  {L"ni", L"に"},
  {L"nh", L"ん", L"h"},
  {L"ng", L"ん", L"g"},
  {L"nf", L"ん", L"f"},
  {L"ne", L"ね"},
  {L"nd", L"ん", L"d"},
  {L"nc", L"ん", L"c"},
  {L"nb", L"ん", L"b"},
  {L"na", L"な"},
  {L"n@", L"ん", L"@"},
  {L"n-", L"ん", L"-"},
  {L"myu", L"みゅ"},
  {L"myo", L"みょ"},
  {L"myi", L"みぃ"},
  {L"mye", L"みぇ"},
  {L"mya", L"みゃ"},
  {L"mu", L"む"},
  {L"mo", L"も"},
  {L"mi", L"み"},
  {L"me", L"め"},
  {L"ma", L"ま"},
  {L"lyu", L"ゅ"},
  {L"lyo", L"ょ"},
  {L"lyi", L"ぃ"},
  {L"lye", L"ぇ"},
  {L"lya", L"ゃ"},
  {L"lwa", L"ゎ"},
  {L"lu", L"ぅ"},
  {L"ltu", L"っ"},
  {L"ltsu", L"っ"},
  {L"lo", L"ぉ"},
  {L"lke", L"ヶ"},
  {L"lka", L"ヵ"},
  {L"li", L"ぃ"},
  {L"le", L"ぇ"},
  {L"la", L"ぁ"},
  {L"kyu", L"きゅ"},
  {L"kyo", L"きょ"},
  {L"kyi", L"きぃ"},
  {L"kye", L"きぇ"},
  {L"kya", L"きゃ"},
  {L"kwu", L"くぅ"},
  {L"kwo", L"くぉ"},
  {L"kwi", L"くぃ"},
  {L"kwe", L"くぇ"},
  {L"kwa", L"くぁ"},
  {L"ku", L"く"},
  {L"ko", L"こ"},
  {L"ki", L"き"},
  {L"ke", L"け"},
  {L"ka", L"か"},
  {L"jyu", L"じゅ"},
  {L"jyo", L"じょ"},
  {L"jyi", L"じぃ"},
  {L"jye", L"じぇ"},
  {L"jya", L"じゃ"},
  {L"ju", L"じゅ"},
  {L"jo", L"じょ"},
  {L"ji", L"じ"},
  {L"je", L"じぇ"},
  {L"ja", L"じゃ"},
  {L"i", L"い"},
  {L"hyu", L"ひゅ"},
  {L"hyo", L"ひょ"},
  {L"hyi", L"ひぃ"},
  {L"hye", L"ひぇ"},
  {L"hya", L"ひゃ"},
  {L"hu", L"ふ"},
  {L"ho", L"ほ"},
  {L"hi", L"ひ"},
  {L"he", L"へ"},
  {L"ha", L"は"},
  {L"gyu", L"ぎゅ"},
  {L"gyo", L"ぎょ"},
  {L"gyi", L"ぎぃ"},
  {L"gye", L"ぎぇ"},
  {L"gya", L"ぎゃ"},
  {L"gwu", L"ぐぅ"},
  {L"gwo", L"ぐぉ"},
  {L"gwi", L"ぐぃ"},
  {L"gwe", L"ぐぇ"},
  {L"gwa", L"ぐぁ"},
  {L"gu", L"ぐ"},
  {L"go", L"ご"},
  {L"gi", L"ぎ"},
  {L"ge", L"げ"},
  {L"ga", L"が"},
  {L"fyu", L"ふゅ"},
  {L"fyo", L"ふょ"},
  {L"fyi", L"ふぃ"},
  {L"fye", L"ふぇ"},
  {L"fya", L"ふゃ"},
  {L"fwu", L"ふぅ"},
  {L"fwo", L"ふぉ"},
  {L"fwi", L"ふぃ"},
  {L"fwe", L"ふぇ"},
  {L"fwa", L"ふぁ"},
  {L"fu", L"ふ"},
  {L"fo", L"ふぉ"},
  {L"fi", L"ふぃ"},
  {L"fe", L"ふぇ"},
  {L"fa", L"ふぁ"},
  {L"e", L"え"},
  {L"dyu", L"ぢゅ"},
  {L"dyo", L"ぢょ"},
  {L"dyi", L"ぢぃ"},
  {L"dye", L"ぢぇ"},
  {L"dya", L"ぢゃ"},
  {L"dwu", L"どぅ"},
  {L"dwo", L"どぉ"},
  {L"dwi", L"どぃ"},
  {L"dwe", L"どぇ"},
  {L"dwa", L"どぁ"},
  {L"du", L"づ"},
  {L"do", L"ど"},
  {L"di", L"ぢ"},
  {L"dhu", L"でゅ"},
  {L"dho", L"でょ"},
  {L"dhi", L"でぃ"},
  {L"dhe", L"でぇ"},
  {L"dha", L"でゃ"},
  {L"de", L"で"},
  {L"da", L"だ"},
  {L"cyu", L"ちゅ"},
  {L"cyo", L"ちょ"},
  {L"cyi", L"ちぃ"},
  {L"cye", L"ちぇ"},
  {L"cya", L"ちゃ"},
  {L"cu", L"く"},
  {L"co", L"こ"},
  {L"ci", L"し"},
  {L"chu", L"ちゅ"},
  {L"cho", L"ちょ"},
  {L"chi", L"ち"},
  {L"che", L"ちぇ"},
  {L"cha", L"ちゃ"},
  {L"ce", L"せ"},
  {L"ca", L"か"},
  {L"byu", L"びゅ"},
  {L"byo", L"びょ"},
  {L"byi", L"びぃ"},
  {L"bye", L"びぇ"},
  {L"bya", L"びゃ"},
  {L"bu", L"ぶ"},
  {L"bo", L"ぼ"},
  {L"bi", L"び"},
  {L"be", L"べ"},
  {L"ba", L"ば"},
  {L"a", L"あ"},
// 【降順にソート】ここまで
};

static TABLEENTRY sokuon_table[] = {
// 【降順にソート】ここから
  {L"zzyu", L"っじゅ"},
  {L"zzyo", L"っじょ"},
  {L"zzyi", L"っじぃ"},
  {L"zzye", L"っじぇ"},
  {L"zzya", L"っじゃ"},
  {L"zzu", L"っず"},
  {L"zzo", L"っぞ"},
  {L"zzi", L"っじ"},
  {L"zze", L"っぜ"},
  {L"zza", L"っざ"},
  {L"yyu", L"っゆ"},
  {L"yyo", L"っよ"},
  {L"yyi", L"っい"},
  {L"yye", L"っいぇ"},
  {L"yya", L"っや"},
  {L"xxyu", L"っゅ"},
  {L"xxyo", L"っょ"},
  {L"xxyi", L"っぃ"},
  {L"xxye", L"っぇ"},
  {L"xxya", L"っゃ"},
  {L"xxwa", L"っゎ"},
  {L"xxu", L"っぅ"},
  {L"xxtu", L"っっ"},
  {L"xxo", L"っぉ"},
  {L"xxn", L"っん"},
  {L"xxke", L"っヶ"},
  {L"xxka", L"っヵ"},
  {L"xxi", L"っぃ"},
  {L"xxe", L"っぇ"},
  {L"xxa", L"っぁ"},
  {L"wwu", L"っう"},
  {L"wwo", L"っを"},
  {L"wwi", L"っうぃ"},
  {L"wwhu", L"っう"},
  {L"wwho", L"っうぉ"},
  {L"wwhi", L"っうぃ"},
  {L"wwhe", L"っうぇ"},
  {L"wwha", L"っうぁ"},
  {L"wwe", L"っうぇ"},
  {L"wwa", L"っわ"},
  {L"vvyu", L"っヴゅ"},
  {L"vvyo", L"っヴょ"},
  {L"vvyi", L"っヴぃ"},
  {L"vvye", L"っヴぇ"},
  {L"vvya", L"っヴゃ"},
  {L"vvu", L"っヴ"},
  {L"vvo", L"っヴぉ"},
  {L"vvi", L"っヴぃ"},
  {L"vve", L"っヴぇ"},
  {L"vva", L"っヴぁ"},
  {L"ttyu", L"っちゅ"},
  {L"ttyo", L"っちょ"},
  {L"ttyi", L"っちぃ"},
  {L"ttye", L"っちぇ"},
  {L"ttya", L"っちゃ"},
  {L"ttwu", L"っとぅ"},
  {L"ttwo", L"っとぉ"},
  {L"ttwi", L"っとぃ"},
  {L"ttwe", L"っとぇ"},
  {L"ttwa", L"っとぁ"},
  {L"ttu", L"っつ"},
  {L"ttsu", L"っつ"},
  {L"ttso", L"っつぉ"},
  {L"ttsi", L"っつぃ"},
  {L"ttse", L"っつぇ"},
  {L"ttsa", L"っつぁ"},
  {L"tto", L"っと"},
  {L"tti", L"っち"},
  {L"tthu", L"ってゅ"},
  {L"ttho", L"ってょ"},
  {L"tthi", L"ってぃ"},
  {L"tthe", L"ってぇ"},
  {L"ttha", L"ってゃ"},
  {L"tte", L"って"},
  {L"tta", L"った"},
  {L"ssyu", L"っしゅ"},
  {L"ssyo", L"っしょ"},
  {L"ssyi", L"っしぃ"},
  {L"ssye", L"っしぇ"},
  {L"ssya", L"っしゃ"},
  {L"sswu", L"っすぅ"},
  {L"sswo", L"っすぉ"},
  {L"sswi", L"っすぃ"},
  {L"sswe", L"っすぇ"},
  {L"sswa", L"っすぁ"},
  {L"ssu", L"っす"},
  {L"sso", L"っそ"},
  {L"ssi", L"っし"},
  {L"sshu", L"っしゅ"},
  {L"ssho", L"っしょ"},
  {L"sshi", L"っし"},
  {L"sshe", L"っしぇ"},
  {L"ssha", L"っしゃ"},
  {L"sse", L"っせ"},
  {L"ssa", L"っさ"},
  {L"rryu", L"っりゅ"},
  {L"rryo", L"っりょ"},
  {L"rryi", L"っりぃ"},
  {L"rrye", L"っりぇ"},
  {L"rrya", L"っりゃ"},
  {L"rru", L"っる"},
  {L"rro", L"っろ"},
  {L"rri", L"っり"},
  {L"rre", L"っれ"},
  {L"rra", L"っら"},
  {L"qqyu", L"っくゅ"},
  {L"qqyo", L"っくょ"},
  {L"qqyi", L"っくぃ"},
  {L"qqye", L"っくぇ"},
  {L"qqya", L"っくゃ"},
  {L"qqwu", L"っくぅ"},
  {L"qqwo", L"っくぉ"},
  {L"qqwi", L"っくぃ"},
  {L"qqwe", L"っくぇ"},
  {L"qqwa", L"っくぁ"},
  {L"qqu", L"っく"},
  {L"qqo", L"っくぉ"},
  {L"qqi", L"っくぃ"},
  {L"qqe", L"っくぇ"},
  {L"qqa", L"っくぁ"},
  {L"ppyu", L"っぴゅ"},
  {L"ppyo", L"っぴょ"},
  {L"ppyi", L"っぴぃ"},
  {L"ppye", L"っぴぇ"},
  {L"ppya", L"っぴゃ"},
  {L"ppu", L"っぷ"},
  {L"ppo", L"っぽ"},
  {L"ppi", L"っぴ"},
  {L"ppe", L"っぺ"},
  {L"ppa", L"っぱ"},
  {L"mmyu", L"っみゅ"},
  {L"mmyo", L"っみょ"},
  {L"mmyi", L"っみぃ"},
  {L"mmye", L"っみぇ"},
  {L"mmya", L"っみゃ"},
  {L"mmu", L"っむ"},
  {L"mmo", L"っも"},
  {L"mmi", L"っみ"},
  {L"mme", L"っめ"},
  {L"mma", L"っま"},
  {L"llyu", L"っゅ"},
  {L"llyo", L"っょ"},
  {L"llyi", L"っぃ"},
  {L"llye", L"っぇ"},
  {L"llya", L"っゃ"},
  {L"llwa", L"っゎ"},
  {L"llu", L"っぅ"},
  {L"lltu", L"っっ"},
  {L"lltsu", L"っっ"},
  {L"llo", L"っぉ"},
  {L"llke", L"っヶ"},
  {L"llka", L"っヵ"},
  {L"lli", L"っぃ"},
  {L"lle", L"っぇ"},
  {L"lla", L"っぁ"},
  {L"kkyu", L"っきゅ"},
  {L"kkyo", L"っきょ"},
  {L"kkyi", L"っきぃ"},
  {L"kkye", L"っきぇ"},
  {L"kkya", L"っきゃ"},
  {L"kkwu", L"っくぅ"},
  {L"kkwo", L"っくぉ"},
  {L"kkwi", L"っくぃ"},
  {L"kkwe", L"っくぇ"},
  {L"kkwa", L"っくぁ"},
  {L"kku", L"っく"},
  {L"kko", L"っこ"},
  {L"kki", L"っき"},
  {L"kke", L"っけ"},
  {L"kka", L"っか"},
  {L"jjyu", L"っじゅ"},
  {L"jjyo", L"っじょ"},
  {L"jjyi", L"っじぃ"},
  {L"jjye", L"っじぇ"},
  {L"jjya", L"っじゃ"},
  {L"jju", L"っじゅ"},
  {L"jjo", L"っじょ"},
  {L"jji", L"っじ"},
  {L"jje", L"っじぇ"},
  {L"jja", L"っじゃ"},
  {L"hhyu", L"っひゅ"},
  {L"hhyo", L"っひょ"},
  {L"hhyi", L"っひぃ"},
  {L"hhye", L"っひぇ"},
  {L"hhya", L"っひゃ"},
  {L"hhu", L"っふ"},
  {L"hho", L"っほ"},
  {L"hhi", L"っひ"},
  {L"hhe", L"っへ"},
  {L"hha", L"っは"},
  {L"ggyu", L"っぎゅ"},
  {L"ggyo", L"っぎょ"},
  {L"ggyi", L"っぎぃ"},
  {L"ggye", L"っぎぇ"},
  {L"ggya", L"っぎゃ"},
  {L"ggwu", L"っぐぅ"},
  {L"ggwo", L"っぐぉ"},
  {L"ggwi", L"っぐぃ"},
  {L"ggwe", L"っぐぇ"},
  {L"ggwa", L"っぐぁ"},
  {L"ggu", L"っぐ"},
  {L"ggo", L"っご"},
  {L"ggi", L"っぎ"},
  {L"gge", L"っげ"},
  {L"gga", L"っが"},
  {L"ffyu", L"っふゅ"},
  {L"ffyo", L"っふょ"},
  {L"ffyi", L"っふぃ"},
  {L"ffye", L"っふぇ"},
  {L"ffya", L"っふゃ"},
  {L"ffwu", L"っふぅ"},
  {L"ffwo", L"っふぉ"},
  {L"ffwi", L"っふぃ"},
  {L"ffwe", L"っふぇ"},
  {L"ffwa", L"っふぁ"},
  {L"ffu", L"っふ"},
  {L"ffo", L"っふぉ"},
  {L"ffi", L"っふぃ"},
  {L"ffe", L"っふぇ"},
  {L"ffa", L"っふぁ"},
  {L"ddyu", L"っぢゅ"},
  {L"ddyo", L"っぢょ"},
  {L"ddyi", L"っぢぃ"},
  {L"ddye", L"っぢぇ"},
  {L"ddya", L"っぢゃ"},
  {L"ddwu", L"っどぅ"},
  {L"ddwo", L"っどぉ"},
  {L"ddwi", L"っどぃ"},
  {L"ddwe", L"っどぇ"},
  {L"ddwa", L"っどぁ"},
  {L"ddu", L"っづ"},
  {L"ddo", L"っど"},
  {L"ddi", L"っぢ"},
  {L"ddhu", L"っでゅ"},
  {L"ddho", L"っでょ"},
  {L"ddhi", L"っでぃ"},
  {L"ddhe", L"っでぇ"},
  {L"ddha", L"っでゃ"},
  {L"dde", L"っで"},
  {L"dda", L"っだ"},
  {L"ccyu", L"っちゅ"},
  {L"ccyo", L"っちょ"},
  {L"ccyi", L"っちぃ"},
  {L"ccye", L"っちぇ"},
  {L"ccya", L"っちゃ"},
  {L"ccu", L"っく"},
  {L"cco", L"っこ"},
  {L"cci", L"っし"},
  {L"cchu", L"っちゅ"},
  {L"ccho", L"っちょ"},
  {L"cchi", L"っち"},
  {L"cche", L"っちぇ"},
  {L"ccha", L"っちゃ"},
  {L"cce", L"っせ"},
  {L"cca", L"っか"},
  {L"bbyu", L"っびゅ"},
  {L"bbyo", L"っびょ"},
  {L"bbyi", L"っびぃ"},
  {L"bbye", L"っびぇ"},
  {L"bbya", L"っびゃ"},
  {L"bbu", L"っぶ"},
  {L"bbo", L"っぼ"},
  {L"bbi", L"っび"},
  {L"bbe", L"っべ"},
  {L"bba", L"っば"},
// 【降順にソート】ここまで
};

//////////////////////////////////////////////////////////////////////////////

std::wstring zenkaku_to_hankaku(const std::wstring& zenkaku) {
  std::wstring hankaku;
  bool flag;
  wchar_t ch;
  for (size_t k = 0; k < zenkaku.size(); ++k) {
    flag = true;
    ch = zenkaku[k];
    for (size_t i = 0; i < _countof(halfkana_table); ++i) {
      if (ch == halfkana_table[i].key[0]) {
        hankaku += halfkana_table[i].value;
        flag = false;
        break;
      }
    }
    if (flag) {
      hankaku += ch;
    }
  }
  for (size_t i = 0; i < _countof(kigou_table); ++i) {
    for (size_t k = 0; k < zenkaku.size(); ++k) {
      if (hankaku[k] == kigou_table[i].value[0]) {
        hankaku[k] = kigou_table[i].key[0];
        break;
      }
    }
  }
  WCHAR szBuf[1024];
  const LCID langid = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
  szBuf[0] = 0;
  DWORD dwFlags = LCMAP_HALFWIDTH;
  ::LCMapStringW(MAKELCID(langid, SORT_DEFAULT), dwFlags,
                 hankaku.c_str(), -1, szBuf, 1024);
  return szBuf;
} // zenkaku_to_hankaku

std::wstring hankaku_to_zenkaku(const std::wstring& hankaku) {
  std::wstring zenkaku = hankaku;
  for (size_t i = 0; i < _countof(kana_table); ++i) {
    unboost::replace_all(zenkaku, kana_table[i].key, kana_table[i].value);
  }
  for (size_t i = 0; i < _countof(kigou_table); ++i) {
    for (size_t k = 0; k < zenkaku.size(); ++k) {
      if (zenkaku[k] == kigou_table[i].key[0]) {
        zenkaku[k] = kigou_table[i].value[0];
        break;
      }
    }
  }
  WCHAR szBuf[1024];
  const LCID langid = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
  szBuf[0] = 0;
  DWORD dwFlags = LCMAP_FULLWIDTH;
  ::LCMapStringW(MAKELCID(langid, SORT_DEFAULT), dwFlags,
                 zenkaku.c_str(), -1, szBuf, 1024);
  return szBuf;
} // hankaku_to_zenkaku

std::wstring zenkaku_hiragana_to_katakana(const std::wstring& hiragana) {
  WCHAR szBuf[1024];
  const LCID langid = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
  szBuf[0] = 0;
  DWORD dwFlags = LCMAP_FULLWIDTH | LCMAP_KATAKANA;
  ::LCMapStringW(MAKELCID(langid, SORT_DEFAULT), dwFlags,
                 hiragana.c_str(), -1, szBuf, 1024);
  return szBuf;
} // zenkaku_hiragana_to_katakana

std::wstring zenkaku_katakana_to_hiragana(const std::wstring& katakana) {
  WCHAR szBuf[1024];
  const LCID langid = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
  szBuf[0] = 0;
  DWORD dwFlags = LCMAP_FULLWIDTH | LCMAP_HIRAGANA;
  ::LCMapStringW(MAKELCID(langid, SORT_DEFAULT), dwFlags,
                 katakana.c_str(), -1, szBuf, 1024);
  return szBuf;
} // zenkaku_katakana_to_hiragana

std::wstring hiragana_to_romaji(const std::wstring& hiragana) {
  std::wstring romaji = hiragana;
  for (size_t i = 0; i < _countof(sokuon_table); ++i) {
    unboost::replace_all(romaji, sokuon_table[i].value, sokuon_table[i].key);
  }
  for (size_t i = 0; i < _countof(reverse_table); ++i) {
    unboost::replace_all(romaji, reverse_table[i].key, reverse_table[i].value);
  }
  for (size_t i = 0; i < _countof(romaji_table); ++i) {
    unboost::replace_all(romaji, romaji_table[i].value, romaji_table[i].key);
  }
  return romaji;
} // hiragana_to_romaji

std::wstring romaji_to_hiragana(const std::wstring& romaji) {
  std::wstring hiragana = romaji;
  for (size_t i = 0; i < _countof(sokuon_table); ++i) {
    unboost::replace_all(hiragana, sokuon_table[i].key, sokuon_table[i].value);
  }
  for (size_t i = 0; i < _countof(romaji_table); ++i) {
    unboost::replace_all(hiragana, romaji_table[i].key, romaji_table[i].value);
  }
  return hiragana;
} // romaji_to_hiragana

WCHAR convert_key_to_kana(BYTE vk, BOOL bShift) {
  switch (vk) {
  case VK_A:          return L'ち';
  case VK_B:          return L'こ';
  case VK_C:          return L'そ';
  case VK_D:          return L'し';
  case VK_E:          return (bShift ? L'ぃ' : L'い');
  case VK_F:          return L'は';
  case VK_G:          return L'き';
  case VK_H:          return L'く';
  case VK_I:          return L'に';
  case VK_J:          return L'ま';
  case VK_K:          return L'の';
  case VK_L:          return L'り';
  case VK_M:          return L'も';
  case VK_N:          return L'み';
  case VK_O:          return L'ら';
  case VK_P:          return L'せ';
  case VK_Q:          return L'た';
  case VK_R:          return L'す';
  case VK_S:          return L'と';
  case VK_T:          return L'か';
  case VK_U:          return L'な';
  case VK_V:          return L'ひ';
  case VK_W:          return L'て';
  case VK_X:          return L'さ';
  case VK_Y:          return L'ん';
  case VK_Z:          return (bShift ? L'っ' : L'つ');
  case VK_0:          return (bShift ? L'を' : L'わ');
  case VK_1:          return L'ぬ';
  case VK_2:          return L'ふ';
  case VK_3:          return (bShift ? L'ぁ' : L'あ');
  case VK_4:          return (bShift ? L'ぅ' : L'う');
  case VK_5:          return (bShift ? L'ぇ' : L'え');
  case VK_6:          return (bShift ? L'ぉ' : L'お');
  case VK_7:          return (bShift ? L'ゃ' : L'や');
  case VK_8:          return (bShift ? L'ゅ' : L'ゆ');
  case VK_9:          return (bShift ? L'ょ' : L'よ');
  case VK_OEM_PLUS:   return L'れ';
  case VK_OEM_102:    return L'ろ';
  case VK_OEM_1:      return L'け';
  case VK_OEM_2:      return (bShift ? L'・' : L'め');
  case VK_OEM_3:      return L'゛';
  case VK_OEM_4:      return (bShift ? L'「' : L'゜');
  case VK_OEM_5:      return L'ー';
  case VK_OEM_6:      return (bShift ? L'」' : L'む');
  case VK_OEM_7:      return L'へ';
  case VK_OEM_COMMA:  return (bShift ? L'、' : L'ね');
  case VK_OEM_PERIOD: return (bShift ? L'。' : L'る');
  default:            return 0;
  }
} // convert_key_to_kana

BOOL is_hiragana(WCHAR ch) {
  if (0x3040 <= ch && ch <= 0x309F) return TRUE;
  switch (ch) {
  case 0x3095: case 0x3096: case 0x3099: case 0x309A:
  case 0x309B: case 0x309C: case 0x309D: case 0x309E:
  case 0x30FC:
    return TRUE;
  default:
    return FALSE;
  }
}

BOOL is_zenkaku_katakana(WCHAR ch) {
  if (0x30A0 <= ch && ch <= 0x30FF) return TRUE;
  switch (ch) {
  case 0x30FD: case 0x30FE: case 0x3099: case 0x309A:
  case 0x309B: case 0x309C: case 0x30FC:
    return TRUE;
  default:
    return FALSE;
  }
}

BOOL is_hankaku_katakana(WCHAR ch) {
  if (0xFF65 <= ch && ch <= 0xFF9F) return TRUE;
  switch (ch) {
  case 0xFF61: case 0xFF62: case 0xFF63: case 0xFF64:
    return TRUE;
  default:
    return FALSE;
  }
}

BOOL is_kanji(WCHAR ch) {
  // CJK統合漢字
  if (0x4E00 <= ch && ch <= 0x9FFF) return TRUE;
  // CJK互換漢字
  if (0xF900 <= ch && ch <= 0xFAFF) return TRUE;
  return FALSE;
}

BOOL is_fullwidth_ascii(WCHAR ch) {
  return (0xFF00 <= ch && ch <= 0xFFEF);
}

void add_romaji_char(std::wstring& strComp, WCHAR ch, DWORD& dwCursorPos) {
  strComp.insert(dwCursorPos, 1, ch);
  ++dwCursorPos;

  for (size_t i = 0; i < _countof(sokuon_table); ++i) {
    const std::wstring& key = sokuon_table[i].key;
    if ((DWORD)key.size() <= dwCursorPos) {
      DWORD i = dwCursorPos - (DWORD)key.size();
      if (strComp.substr(i, key.size()) == key) {
        strComp.replace(i, key.size(), sokuon_table[i].value);
        return;
      }
    }
  }

  for (size_t i = 0; i < _countof(romaji_table); ++i) {
    const std::wstring& key = romaji_table[i].key;
    if ((DWORD)key.size() <= dwCursorPos) {
      DWORD i = dwCursorPos - (DWORD)key.size();
      if (strComp.substr(i, key.size()) == key) {
        strComp.replace(i, key.size(), romaji_table[i].value);
        return;
      }
    }
  }
} // add_romaji_char

void add_hiragana_char(std::wstring& strComp, WCHAR ch, DWORD& dwCursorPos) {
  if (is_zenkaku_katakana(ch)) {
    WCHAR sz[2] = {ch, 0};
    const LCID langid = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
    sz[0] = 0;
    DWORD dwFlags = LCMAP_FULLWIDTH | LCMAP_HIRAGANA;
    ::LCMapStringW(MAKELCID(langid, SORT_DEFAULT), dwFlags,
                   sz, 1, &ch, 1);
  }

  strComp.insert(dwCursorPos, 1, ch);
  ++dwCursorPos;
} // add_hiragana_char

void add_katakana_char(std::wstring& strComp, WCHAR ch, DWORD& dwCursorPos) {
  if (is_hiragana(ch)) {
    WCHAR sz[2] = {ch, 0};
    const LCID langid = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
    sz[0] = 0;
    DWORD dwFlags = LCMAP_FULLWIDTH | LCMAP_KATAKANA;
    ::LCMapStringW(MAKELCID(langid, SORT_DEFAULT), dwFlags,
                   sz, 1, &ch, 1);
  }

  strComp.insert(dwCursorPos, 1, ch);
  ++dwCursorPos;
} // add_katakana_char

void add_ascii_char(std::wstring& strComp, WCHAR ch, DWORD& dwCursorPos) {
  strComp.insert(dwCursorPos, 1, ch);
  ++dwCursorPos;
}

//////////////////////////////////////////////////////////////////////////////
