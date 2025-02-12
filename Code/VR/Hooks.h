#pragma once

#include <cstdint>
#include <string>

#define IMPL_ORIG(hModule, fnName) static auto orig_##fnName = vrperfkit::hooks::LoadFunction(hModule, #fnName, fnName)

#ifdef _WIN64
#define STDCALL
#define FASTCALL
#else
#define STDCALL __stdcall
#define FASTCALL __fastcall
#endif

namespace hooks {
	bool Init();
	void Shutdown();

	/*template<typename T>
	T LoadFunction(HMODULE module, const std::string &name, T) {
		auto fn = GetProcAddress(module, name.c_str());
		if (fn == nullptr) {
			LOG_ERROR << "Could not load function " << name;
		}
		return reinterpret_cast<T>(fn);
	}*/

	void InstallHook(const std::string &name, void *target, void *detour);
	//void InstallHookInDll(const std::string &name, HMODULE module, void *detour);
	void InstallVirtualFunctionHook(const std::string &name, void *instance, uint32_t methodPos, void *detour);
	void RemoveHook(void *detour);

	intptr_t HookToOriginal(intptr_t hook);

	template<typename T>
	T CallOriginal(T hookFunction) {
		return reinterpret_cast<T>(HookToOriginal(reinterpret_cast<intptr_t>(hookFunction)));
	}

	template <typename T, typename R, typename... Args>
	int getVTableIndex(R (T::*func)(Args...))
	{
		struct Hooks_VTable
		{
			virtual int Get000() { return   0; }
			virtual int Get001() { return   1; }
			virtual int Get002() { return   2; }
			virtual int Get003() { return   3; }
			virtual int Get004() { return   4; }
			virtual int Get005() { return   5; }
			virtual int Get006() { return   6; }
			virtual int Get007() { return   7; }
			virtual int Get008() { return   8; }
			virtual int Get009() { return   9; }
			virtual int Get010() { return  10; }
			virtual int Get011() { return  11; }
			virtual int Get012() { return  12; }
			virtual int Get013() { return  13; }
			virtual int Get014() { return  14; }
			virtual int Get015() { return  15; }
			virtual int Get016() { return  16; }
			virtual int Get017() { return  17; }
			virtual int Get018() { return  18; }
			virtual int Get019() { return  19; }
			virtual int Get020() { return  20; }
			virtual int Get021() { return  21; }
			virtual int Get022() { return  22; }
			virtual int Get023() { return  23; }
			virtual int Get024() { return  24; }
			virtual int Get025() { return  25; }
			virtual int Get026() { return  26; }
			virtual int Get027() { return  27; }
			virtual int Get028() { return  28; }
			virtual int Get029() { return  29; }
			virtual int Get030() { return  30; }
			virtual int Get031() { return  31; }
			virtual int Get032() { return  32; }
			virtual int Get033() { return  33; }
			virtual int Get034() { return  34; }
			virtual int Get035() { return  35; }
			virtual int Get036() { return  36; }
			virtual int Get037() { return  37; }
			virtual int Get038() { return  38; }
			virtual int Get039() { return  39; }
			virtual int Get040() { return  40; }
			virtual int Get041() { return  41; }
			virtual int Get042() { return  42; }
			virtual int Get043() { return  43; }
			virtual int Get044() { return  44; }
			virtual int Get045() { return  45; }
			virtual int Get046() { return  46; }
			virtual int Get047() { return  47; }
			virtual int Get048() { return  48; }
			virtual int Get049() { return  49; }
			virtual int Get050() { return  50; }
			virtual int Get051() { return  51; }
			virtual int Get052() { return  52; }
			virtual int Get053() { return  53; }
			virtual int Get054() { return  54; }
			virtual int Get055() { return  55; }
			virtual int Get056() { return  56; }
			virtual int Get057() { return  57; }
			virtual int Get058() { return  58; }
			virtual int Get059() { return  59; }
			virtual int Get060() { return  60; }
			virtual int Get061() { return  61; }
			virtual int Get062() { return  62; }
			virtual int Get063() { return  63; }
			virtual int Get064() { return  64; }
			virtual int Get065() { return  65; }
			virtual int Get066() { return  66; }
			virtual int Get067() { return  67; }
			virtual int Get068() { return  68; }
			virtual int Get069() { return  69; }
			virtual int Get070() { return  70; }
			virtual int Get071() { return  71; }
			virtual int Get072() { return  72; }
			virtual int Get073() { return  73; }
			virtual int Get074() { return  74; }
			virtual int Get075() { return  75; }
			virtual int Get076() { return  76; }
			virtual int Get077() { return  77; }
			virtual int Get078() { return  78; }
			virtual int Get079() { return  79; }
			virtual int Get080() { return  80; }
			virtual int Get081() { return  81; }
			virtual int Get082() { return  82; }
			virtual int Get083() { return  83; }
			virtual int Get084() { return  84; }
			virtual int Get085() { return  85; }
			virtual int Get086() { return  86; }
			virtual int Get087() { return  87; }
			virtual int Get088() { return  88; }
			virtual int Get089() { return  89; }
			virtual int Get090() { return  90; }
			virtual int Get091() { return  91; }
			virtual int Get092() { return  92; }
			virtual int Get093() { return  93; }
			virtual int Get094() { return  94; }
			virtual int Get095() { return  95; }
			virtual int Get096() { return  96; }
			virtual int Get097() { return  97; }
			virtual int Get098() { return  98; }
			virtual int Get099() { return  99; }
			virtual int Get100() { return 100; }
			virtual int Get101() { return 101; }
			virtual int Get102() { return 102; }
			virtual int Get103() { return 103; }
			virtual int Get104() { return 104; }
			virtual int Get105() { return 105; }
			virtual int Get106() { return 106; }
			virtual int Get107() { return 107; }
			virtual int Get108() { return 108; }
			virtual int Get109() { return 109; }
			virtual int Get110() { return 110; }
			virtual int Get111() { return 111; }
			virtual int Get112() { return 112; }
			virtual int Get113() { return 113; }
			virtual int Get114() { return 114; }
			virtual int Get115() { return 115; }
			virtual int Get116() { return 116; }
			virtual int Get117() { return 117; }
			virtual int Get118() { return 118; }
			virtual int Get119() { return 119; }
			virtual int Get120() { return 120; }
			virtual int Get121() { return 121; }
			virtual int Get122() { return 122; }
			virtual int Get123() { return 123; }
			virtual int Get124() { return 124; }
			virtual int Get125() { return 125; }
			virtual int Get126() { return 126; }
			virtual int Get127() { return 127; }
			virtual int Get128() { return 128; }
			virtual int Get129() { return 129; }
			virtual int Get130() { return 130; }
			virtual int Get131() { return 131; }
			virtual int Get132() { return 132; }
			virtual int Get133() { return 133; }
			virtual int Get134() { return 134; }
			virtual int Get135() { return 135; }
			virtual int Get136() { return 136; }
			virtual int Get137() { return 137; }
			virtual int Get138() { return 138; }
			virtual int Get139() { return 139; }
			virtual int Get140() { return 140; }
			virtual int Get141() { return 141; }
			virtual int Get142() { return 142; }
			virtual int Get143() { return 143; }
			virtual int Get144() { return 144; }
			virtual int Get145() { return 145; }
			virtual int Get146() { return 146; }
			virtual int Get147() { return 147; }
			virtual int Get148() { return 148; }
			virtual int Get149() { return 149; }
			virtual int Get150() { return 150; }
			virtual int Get151() { return 151; }
			virtual int Get152() { return 152; }
			virtual int Get153() { return 153; }
			virtual int Get154() { return 154; }
			virtual int Get155() { return 155; }
			virtual int Get156() { return 156; }
			virtual int Get157() { return 157; }
			virtual int Get158() { return 158; }
			virtual int Get159() { return 159; }
			virtual int Get160() { return 160; }
			virtual int Get161() { return 161; }
			virtual int Get162() { return 162; }
			virtual int Get163() { return 163; }
			virtual int Get164() { return 164; }
			virtual int Get165() { return 165; }
			virtual int Get166() { return 166; }
			virtual int Get167() { return 167; }
			virtual int Get168() { return 168; }
			virtual int Get169() { return 169; }
			virtual int Get170() { return 170; }
			virtual int Get171() { return 171; }
			virtual int Get172() { return 172; }
			virtual int Get173() { return 173; }
			virtual int Get174() { return 174; }
			virtual int Get175() { return 175; }
			virtual int Get176() { return 176; }
			virtual int Get177() { return 177; }
			virtual int Get178() { return 178; }
			virtual int Get179() { return 179; }
			virtual int Get180() { return 180; }
			virtual int Get181() { return 181; }
			virtual int Get182() { return 182; }
			virtual int Get183() { return 183; }
			virtual int Get184() { return 184; }
			virtual int Get185() { return 185; }
			virtual int Get186() { return 186; }
			virtual int Get187() { return 187; }
			virtual int Get188() { return 188; }
			virtual int Get189() { return 189; }
			virtual int Get190() { return 190; }
			virtual int Get191() { return 191; }
			virtual int Get192() { return 192; }
			virtual int Get193() { return 193; }
			virtual int Get194() { return 194; }
			virtual int Get195() { return 195; }
			virtual int Get196() { return 196; }
			virtual int Get197() { return 197; }
			virtual int Get198() { return 198; }
			virtual int Get199() { return 199; }
			virtual int Get200() { return 200; }
			virtual int Get201() { return 201; }
			virtual int Get202() { return 202; }
			virtual int Get203() { return 203; }
			virtual int Get204() { return 204; }
			virtual int Get205() { return 205; }
			virtual int Get206() { return 206; }
			virtual int Get207() { return 207; }
			virtual int Get208() { return 208; }
			virtual int Get209() { return 209; }
			virtual int Get210() { return 210; }
			virtual int Get211() { return 211; }
			virtual int Get212() { return 212; }
			virtual int Get213() { return 213; }
			virtual int Get214() { return 214; }
			virtual int Get215() { return 215; }
			virtual int Get216() { return 216; }
			virtual int Get217() { return 217; }
			virtual int Get218() { return 218; }
			virtual int Get219() { return 219; }
			virtual int Get220() { return 220; }
			virtual int Get221() { return 221; }
			virtual int Get222() { return 222; }
			virtual int Get223() { return 223; }
			virtual int Get224() { return 224; }
			virtual int Get225() { return 225; }
			virtual int Get226() { return 226; }
			virtual int Get227() { return 227; }
			virtual int Get228() { return 228; }
			virtual int Get229() { return 229; }
			virtual int Get230() { return 230; }
			virtual int Get231() { return 231; }
			virtual int Get232() { return 232; }
			virtual int Get233() { return 233; }
			virtual int Get234() { return 234; }
			virtual int Get235() { return 235; }
			virtual int Get236() { return 236; }
			virtual int Get237() { return 237; }
			virtual int Get238() { return 238; }
			virtual int Get239() { return 239; }
			virtual int Get240() { return 240; }
			virtual int Get241() { return 241; }
			virtual int Get242() { return 242; }
			virtual int Get243() { return 243; }
			virtual int Get244() { return 244; }
			virtual int Get245() { return 245; }
			virtual int Get246() { return 246; }
			virtual int Get247() { return 247; }
			virtual int Get248() { return 248; }
			virtual int Get249() { return 249; }
			virtual int Get250() { return 250; }
			virtual int Get251() { return 251; }
			virtual int Get252() { return 252; }
			virtual int Get253() { return 253; }
			virtual int Get254() { return 254; }
			virtual int Get255() { return 255; }
			virtual int Get256() { return 256; }
			virtual int Get257() { return 257; }
			virtual int Get258() { return 258; }
			virtual int Get259() { return 259; }
			virtual int Get260() { return 260; }
			virtual int Get261() { return 261; }
			virtual int Get262() { return 262; }
			virtual int Get263() { return 263; }
			virtual int Get264() { return 264; }
			virtual int Get265() { return 265; }
			virtual int Get266() { return 266; }
			virtual int Get267() { return 267; }
			virtual int Get268() { return 268; }
			virtual int Get269() { return 269; }
			virtual int Get270() { return 270; }
			virtual int Get271() { return 271; }
			virtual int Get272() { return 272; }
			virtual int Get273() { return 273; }
			virtual int Get274() { return 274; }
			virtual int Get275() { return 275; }
			virtual int Get276() { return 276; }
			virtual int Get277() { return 277; }
			virtual int Get278() { return 278; }
			virtual int Get279() { return 279; }
			virtual int Get280() { return 280; }
			virtual int Get281() { return 281; }
			virtual int Get282() { return 282; }
			virtual int Get283() { return 283; }
			virtual int Get284() { return 284; }
			virtual int Get285() { return 285; }
			virtual int Get286() { return 286; }
			virtual int Get287() { return 287; }
			virtual int Get288() { return 288; }
			virtual int Get289() { return 289; }
			virtual int Get290() { return 290; }
			virtual int Get291() { return 291; }
			virtual int Get292() { return 292; }
			virtual int Get293() { return 293; }
			virtual int Get294() { return 294; }
			virtual int Get295() { return 295; }
			virtual int Get296() { return 296; }
			virtual int Get297() { return 297; }
			virtual int Get298() { return 298; }
			virtual int Get299() { return 299; }
			virtual int Get300() { return 300; }
			virtual int Get301() { return 301; }
			virtual int Get302() { return 302; }
			virtual int Get303() { return 303; }
			virtual int Get304() { return 304; }
			virtual int Get305() { return 305; }
			virtual int Get306() { return 306; }
			virtual int Get307() { return 307; }
			virtual int Get308() { return 308; }
			virtual int Get309() { return 309; }
			virtual int Get310() { return 310; }
			virtual int Get311() { return 311; }
			virtual int Get312() { return 312; }
			virtual int Get313() { return 313; }
			virtual int Get314() { return 314; }
			virtual int Get315() { return 315; }
			virtual int Get316() { return 316; }
			virtual int Get317() { return 317; }
			virtual int Get318() { return 318; }
			virtual int Get319() { return 319; }
			virtual int Get320() { return 320; }
			virtual int Get321() { return 321; }
			virtual int Get322() { return 322; }
			virtual int Get323() { return 323; }
			virtual int Get324() { return 324; }
			virtual int Get325() { return 325; }
			virtual int Get326() { return 326; }
			virtual int Get327() { return 327; }
			virtual int Get328() { return 328; }
			virtual int Get329() { return 329; }
			virtual int Get330() { return 330; }
			virtual int Get331() { return 331; }
			virtual int Get332() { return 332; }
			virtual int Get333() { return 333; }
			virtual int Get334() { return 334; }
			virtual int Get335() { return 335; }
			virtual int Get336() { return 336; }
			virtual int Get337() { return 337; }
			virtual int Get338() { return 338; }
			virtual int Get339() { return 339; }
			virtual int Get340() { return 340; }
			virtual int Get341() { return 341; }
			virtual int Get342() { return 342; }
			virtual int Get343() { return 343; }
			virtual int Get344() { return 344; }
			virtual int Get345() { return 345; }
			virtual int Get346() { return 346; }
			virtual int Get347() { return 347; }
			virtual int Get348() { return 348; }
			virtual int Get349() { return 349; }
			virtual int Get350() { return 350; }
			virtual int Get351() { return 351; }
			virtual int Get352() { return 352; }
			virtual int Get353() { return 353; }
			virtual int Get354() { return 354; }
			virtual int Get355() { return 355; }
			virtual int Get356() { return 356; }
			virtual int Get357() { return 357; }
			virtual int Get358() { return 358; }
			virtual int Get359() { return 359; }
			virtual int Get360() { return 360; }
			virtual int Get361() { return 361; }
			virtual int Get362() { return 362; }
			virtual int Get363() { return 363; }
			virtual int Get364() { return 364; }
			virtual int Get365() { return 365; }
			virtual int Get366() { return 366; }
			virtual int Get367() { return 367; }
			virtual int Get368() { return 368; }
			virtual int Get369() { return 369; }
			virtual int Get370() { return 370; }
			virtual int Get371() { return 371; }
			virtual int Get372() { return 372; }
			virtual int Get373() { return 373; }
			virtual int Get374() { return 374; }
			virtual int Get375() { return 375; }
			virtual int Get376() { return 376; }
			virtual int Get377() { return 377; }
			virtual int Get378() { return 378; }
			virtual int Get379() { return 379; }
			virtual int Get380() { return 380; }
			virtual int Get381() { return 381; }
			virtual int Get382() { return 382; }
			virtual int Get383() { return 383; }
			virtual int Get384() { return 384; }
			virtual int Get385() { return 385; }
			virtual int Get386() { return 386; }
			virtual int Get387() { return 387; }
			virtual int Get388() { return 388; }
			virtual int Get389() { return 389; }
			virtual int Get390() { return 390; }
			virtual int Get391() { return 391; }
			virtual int Get392() { return 392; }
			virtual int Get393() { return 393; }
			virtual int Get394() { return 394; }
			virtual int Get395() { return 395; }
			virtual int Get396() { return 396; }
			virtual int Get397() { return 397; }
			virtual int Get398() { return 398; }
			virtual int Get399() { return 399; }
			virtual int Get400() { return 400; }
			virtual int Get401() { return 401; }
			virtual int Get402() { return 402; }
			virtual int Get403() { return 403; }
			virtual int Get404() { return 404; }
			virtual int Get405() { return 405; }
			virtual int Get406() { return 406; }
			virtual int Get407() { return 407; }
			virtual int Get408() { return 408; }
			virtual int Get409() { return 409; }
			virtual int Get410() { return 410; }
			virtual int Get411() { return 411; }
			virtual int Get412() { return 412; }
			virtual int Get413() { return 413; }
			virtual int Get414() { return 414; }
			virtual int Get415() { return 415; }
			virtual int Get416() { return 416; }
			virtual int Get417() { return 417; }
			virtual int Get418() { return 418; }
			virtual int Get419() { return 419; }
			virtual int Get420() { return 420; }
			virtual int Get421() { return 421; }
			virtual int Get422() { return 422; }
			virtual int Get423() { return 423; }
			virtual int Get424() { return 424; }
			virtual int Get425() { return 425; }
			virtual int Get426() { return 426; }
			virtual int Get427() { return 427; }
			virtual int Get428() { return 428; }
			virtual int Get429() { return 429; }
			virtual int Get430() { return 430; }
			virtual int Get431() { return 431; }
			virtual int Get432() { return 432; }
			virtual int Get433() { return 433; }
			virtual int Get434() { return 434; }
			virtual int Get435() { return 435; }
			virtual int Get436() { return 436; }
			virtual int Get437() { return 437; }
			virtual int Get438() { return 438; }
			virtual int Get439() { return 439; }
			virtual int Get440() { return 440; }
			virtual int Get441() { return 441; }
			virtual int Get442() { return 442; }
			virtual int Get443() { return 443; }
			virtual int Get444() { return 444; }
			virtual int Get445() { return 445; }
			virtual int Get446() { return 446; }
			virtual int Get447() { return 447; }
			virtual int Get448() { return 448; }
			virtual int Get449() { return 449; }
			virtual int Get450() { return 450; }
			virtual int Get451() { return 451; }
			virtual int Get452() { return 452; }
			virtual int Get453() { return 453; }
			virtual int Get454() { return 454; }
			virtual int Get455() { return 455; }
			virtual int Get456() { return 456; }
			virtual int Get457() { return 457; }
			virtual int Get458() { return 458; }
			virtual int Get459() { return 459; }
			virtual int Get460() { return 460; }
			virtual int Get461() { return 461; }
			virtual int Get462() { return 462; }
			virtual int Get463() { return 463; }
			virtual int Get464() { return 464; }
			virtual int Get465() { return 465; }
			virtual int Get466() { return 466; }
			virtual int Get467() { return 467; }
			virtual int Get468() { return 468; }
			virtual int Get469() { return 469; }
			virtual int Get470() { return 470; }
			virtual int Get471() { return 471; }
			virtual int Get472() { return 472; }
			virtual int Get473() { return 473; }
			virtual int Get474() { return 474; }
			virtual int Get475() { return 475; }
			virtual int Get476() { return 476; }
			virtual int Get477() { return 477; }
			virtual int Get478() { return 478; }
			virtual int Get479() { return 479; }
			virtual int Get480() { return 480; }
			virtual int Get481() { return 481; }
			virtual int Get482() { return 482; }
			virtual int Get483() { return 483; }
			virtual int Get484() { return 484; }
			virtual int Get485() { return 485; }
			virtual int Get486() { return 486; }
			virtual int Get487() { return 487; }
			virtual int Get488() { return 488; }
			virtual int Get489() { return 489; }
			virtual int Get490() { return 490; }
			virtual int Get491() { return 491; }
			virtual int Get492() { return 492; }
			virtual int Get493() { return 493; }
			virtual int Get494() { return 494; }
			virtual int Get495() { return 495; }
			virtual int Get496() { return 496; }
			virtual int Get497() { return 497; }
			virtual int Get498() { return 498; }
			virtual int Get499() { return 499; }
		} _hooks_vtable;

		T* t = reinterpret_cast<T*>(&_hooks_vtable);
		typedef int (T::* GetIndex)();
		GetIndex getIndex = (GetIndex)func;
		return (t->*getIndex)();
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (T::*func)(Args...), R (*detour)(T*, Args...))
	{
		int vtableIndex = getVTableIndex(func);
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (T::*func)(Args...) const, R (*detour)(T*, Args...))
	{
		int vtableIndex = getVTableIndex(reinterpret_cast<R (T::*)(Args...)>(func));
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}

#ifndef _WIN64
	template <typename T, typename R, typename... Args>
	int getVTableIndex(R (__stdcall T::*func)(Args...))
	{
		struct Hooks_VTable
		{
			virtual int __stdcall Get000() { return   0; }
			virtual int __stdcall Get001() { return   1; }
			virtual int __stdcall Get002() { return   2; }
			virtual int __stdcall Get003() { return   3; }
			virtual int __stdcall Get004() { return   4; }
			virtual int __stdcall Get005() { return   5; }
			virtual int __stdcall Get006() { return   6; }
			virtual int __stdcall Get007() { return   7; }
			virtual int __stdcall Get008() { return   8; }
			virtual int __stdcall Get009() { return   9; }
			virtual int __stdcall Get010() { return  10; }
			virtual int __stdcall Get011() { return  11; }
			virtual int __stdcall Get012() { return  12; }
			virtual int __stdcall Get013() { return  13; }
			virtual int __stdcall Get014() { return  14; }
			virtual int __stdcall Get015() { return  15; }
			virtual int __stdcall Get016() { return  16; }
			virtual int __stdcall Get017() { return  17; }
			virtual int __stdcall Get018() { return  18; }
			virtual int __stdcall Get019() { return  19; }
			virtual int __stdcall Get020() { return  20; }
			virtual int __stdcall Get021() { return  21; }
			virtual int __stdcall Get022() { return  22; }
			virtual int __stdcall Get023() { return  23; }
			virtual int __stdcall Get024() { return  24; }
			virtual int __stdcall Get025() { return  25; }
			virtual int __stdcall Get026() { return  26; }
			virtual int __stdcall Get027() { return  27; }
			virtual int __stdcall Get028() { return  28; }
			virtual int __stdcall Get029() { return  29; }
			virtual int __stdcall Get030() { return  30; }
			virtual int __stdcall Get031() { return  31; }
			virtual int __stdcall Get032() { return  32; }
			virtual int __stdcall Get033() { return  33; }
			virtual int __stdcall Get034() { return  34; }
			virtual int __stdcall Get035() { return  35; }
			virtual int __stdcall Get036() { return  36; }
			virtual int __stdcall Get037() { return  37; }
			virtual int __stdcall Get038() { return  38; }
			virtual int __stdcall Get039() { return  39; }
			virtual int __stdcall Get040() { return  40; }
			virtual int __stdcall Get041() { return  41; }
			virtual int __stdcall Get042() { return  42; }
			virtual int __stdcall Get043() { return  43; }
			virtual int __stdcall Get044() { return  44; }
			virtual int __stdcall Get045() { return  45; }
			virtual int __stdcall Get046() { return  46; }
			virtual int __stdcall Get047() { return  47; }
			virtual int __stdcall Get048() { return  48; }
			virtual int __stdcall Get049() { return  49; }
			virtual int __stdcall Get050() { return  50; }
			virtual int __stdcall Get051() { return  51; }
			virtual int __stdcall Get052() { return  52; }
			virtual int __stdcall Get053() { return  53; }
			virtual int __stdcall Get054() { return  54; }
			virtual int __stdcall Get055() { return  55; }
			virtual int __stdcall Get056() { return  56; }
			virtual int __stdcall Get057() { return  57; }
			virtual int __stdcall Get058() { return  58; }
			virtual int __stdcall Get059() { return  59; }
			virtual int __stdcall Get060() { return  60; }
			virtual int __stdcall Get061() { return  61; }
			virtual int __stdcall Get062() { return  62; }
			virtual int __stdcall Get063() { return  63; }
			virtual int __stdcall Get064() { return  64; }
			virtual int __stdcall Get065() { return  65; }
			virtual int __stdcall Get066() { return  66; }
			virtual int __stdcall Get067() { return  67; }
			virtual int __stdcall Get068() { return  68; }
			virtual int __stdcall Get069() { return  69; }
			virtual int __stdcall Get070() { return  70; }
			virtual int __stdcall Get071() { return  71; }
			virtual int __stdcall Get072() { return  72; }
			virtual int __stdcall Get073() { return  73; }
			virtual int __stdcall Get074() { return  74; }
			virtual int __stdcall Get075() { return  75; }
			virtual int __stdcall Get076() { return  76; }
			virtual int __stdcall Get077() { return  77; }
			virtual int __stdcall Get078() { return  78; }
			virtual int __stdcall Get079() { return  79; }
			virtual int __stdcall Get080() { return  80; }
			virtual int __stdcall Get081() { return  81; }
			virtual int __stdcall Get082() { return  82; }
			virtual int __stdcall Get083() { return  83; }
			virtual int __stdcall Get084() { return  84; }
			virtual int __stdcall Get085() { return  85; }
			virtual int __stdcall Get086() { return  86; }
			virtual int __stdcall Get087() { return  87; }
			virtual int __stdcall Get088() { return  88; }
			virtual int __stdcall Get089() { return  89; }
			virtual int __stdcall Get090() { return  90; }
			virtual int __stdcall Get091() { return  91; }
			virtual int __stdcall Get092() { return  92; }
			virtual int __stdcall Get093() { return  93; }
			virtual int __stdcall Get094() { return  94; }
			virtual int __stdcall Get095() { return  95; }
			virtual int __stdcall Get096() { return  96; }
			virtual int __stdcall Get097() { return  97; }
			virtual int __stdcall Get098() { return  98; }
			virtual int __stdcall Get099() { return  99; }
			virtual int __stdcall Get100() { return 100; }
			virtual int __stdcall Get101() { return 101; }
			virtual int __stdcall Get102() { return 102; }
			virtual int __stdcall Get103() { return 103; }
			virtual int __stdcall Get104() { return 104; }
			virtual int __stdcall Get105() { return 105; }
			virtual int __stdcall Get106() { return 106; }
			virtual int __stdcall Get107() { return 107; }
			virtual int __stdcall Get108() { return 108; }
			virtual int __stdcall Get109() { return 109; }
			virtual int __stdcall Get110() { return 110; }
			virtual int __stdcall Get111() { return 111; }
			virtual int __stdcall Get112() { return 112; }
			virtual int __stdcall Get113() { return 113; }
			virtual int __stdcall Get114() { return 114; }
			virtual int __stdcall Get115() { return 115; }
			virtual int __stdcall Get116() { return 116; }
			virtual int __stdcall Get117() { return 117; }
			virtual int __stdcall Get118() { return 118; }
			virtual int __stdcall Get119() { return 119; }
			virtual int __stdcall Get120() { return 120; }
			virtual int __stdcall Get121() { return 121; }
			virtual int __stdcall Get122() { return 122; }
			virtual int __stdcall Get123() { return 123; }
			virtual int __stdcall Get124() { return 124; }
			virtual int __stdcall Get125() { return 125; }
			virtual int __stdcall Get126() { return 126; }
			virtual int __stdcall Get127() { return 127; }
			virtual int __stdcall Get128() { return 128; }
			virtual int __stdcall Get129() { return 129; }
			virtual int __stdcall Get130() { return 130; }
			virtual int __stdcall Get131() { return 131; }
			virtual int __stdcall Get132() { return 132; }
			virtual int __stdcall Get133() { return 133; }
			virtual int __stdcall Get134() { return 134; }
			virtual int __stdcall Get135() { return 135; }
			virtual int __stdcall Get136() { return 136; }
			virtual int __stdcall Get137() { return 137; }
			virtual int __stdcall Get138() { return 138; }
			virtual int __stdcall Get139() { return 139; }
			virtual int __stdcall Get140() { return 140; }
			virtual int __stdcall Get141() { return 141; }
			virtual int __stdcall Get142() { return 142; }
			virtual int __stdcall Get143() { return 143; }
			virtual int __stdcall Get144() { return 144; }
			virtual int __stdcall Get145() { return 145; }
			virtual int __stdcall Get146() { return 146; }
			virtual int __stdcall Get147() { return 147; }
			virtual int __stdcall Get148() { return 148; }
			virtual int __stdcall Get149() { return 149; }
			virtual int __stdcall Get150() { return 150; }
			virtual int __stdcall Get151() { return 151; }
			virtual int __stdcall Get152() { return 152; }
			virtual int __stdcall Get153() { return 153; }
			virtual int __stdcall Get154() { return 154; }
			virtual int __stdcall Get155() { return 155; }
			virtual int __stdcall Get156() { return 156; }
			virtual int __stdcall Get157() { return 157; }
			virtual int __stdcall Get158() { return 158; }
			virtual int __stdcall Get159() { return 159; }
			virtual int __stdcall Get160() { return 160; }
			virtual int __stdcall Get161() { return 161; }
			virtual int __stdcall Get162() { return 162; }
			virtual int __stdcall Get163() { return 163; }
			virtual int __stdcall Get164() { return 164; }
			virtual int __stdcall Get165() { return 165; }
			virtual int __stdcall Get166() { return 166; }
			virtual int __stdcall Get167() { return 167; }
			virtual int __stdcall Get168() { return 168; }
			virtual int __stdcall Get169() { return 169; }
			virtual int __stdcall Get170() { return 170; }
			virtual int __stdcall Get171() { return 171; }
			virtual int __stdcall Get172() { return 172; }
			virtual int __stdcall Get173() { return 173; }
			virtual int __stdcall Get174() { return 174; }
			virtual int __stdcall Get175() { return 175; }
			virtual int __stdcall Get176() { return 176; }
			virtual int __stdcall Get177() { return 177; }
			virtual int __stdcall Get178() { return 178; }
			virtual int __stdcall Get179() { return 179; }
			virtual int __stdcall Get180() { return 180; }
			virtual int __stdcall Get181() { return 181; }
			virtual int __stdcall Get182() { return 182; }
			virtual int __stdcall Get183() { return 183; }
			virtual int __stdcall Get184() { return 184; }
			virtual int __stdcall Get185() { return 185; }
			virtual int __stdcall Get186() { return 186; }
			virtual int __stdcall Get187() { return 187; }
			virtual int __stdcall Get188() { return 188; }
			virtual int __stdcall Get189() { return 189; }
			virtual int __stdcall Get190() { return 190; }
			virtual int __stdcall Get191() { return 191; }
			virtual int __stdcall Get192() { return 192; }
			virtual int __stdcall Get193() { return 193; }
			virtual int __stdcall Get194() { return 194; }
			virtual int __stdcall Get195() { return 195; }
			virtual int __stdcall Get196() { return 196; }
			virtual int __stdcall Get197() { return 197; }
			virtual int __stdcall Get198() { return 198; }
			virtual int __stdcall Get199() { return 199; }
			virtual int __stdcall Get200() { return 200; }
			virtual int __stdcall Get201() { return 201; }
			virtual int __stdcall Get202() { return 202; }
			virtual int __stdcall Get203() { return 203; }
			virtual int __stdcall Get204() { return 204; }
			virtual int __stdcall Get205() { return 205; }
			virtual int __stdcall Get206() { return 206; }
			virtual int __stdcall Get207() { return 207; }
			virtual int __stdcall Get208() { return 208; }
			virtual int __stdcall Get209() { return 209; }
			virtual int __stdcall Get210() { return 210; }
			virtual int __stdcall Get211() { return 211; }
			virtual int __stdcall Get212() { return 212; }
			virtual int __stdcall Get213() { return 213; }
			virtual int __stdcall Get214() { return 214; }
			virtual int __stdcall Get215() { return 215; }
			virtual int __stdcall Get216() { return 216; }
			virtual int __stdcall Get217() { return 217; }
			virtual int __stdcall Get218() { return 218; }
			virtual int __stdcall Get219() { return 219; }
			virtual int __stdcall Get220() { return 220; }
			virtual int __stdcall Get221() { return 221; }
			virtual int __stdcall Get222() { return 222; }
			virtual int __stdcall Get223() { return 223; }
			virtual int __stdcall Get224() { return 224; }
			virtual int __stdcall Get225() { return 225; }
			virtual int __stdcall Get226() { return 226; }
			virtual int __stdcall Get227() { return 227; }
			virtual int __stdcall Get228() { return 228; }
			virtual int __stdcall Get229() { return 229; }
			virtual int __stdcall Get230() { return 230; }
			virtual int __stdcall Get231() { return 231; }
			virtual int __stdcall Get232() { return 232; }
			virtual int __stdcall Get233() { return 233; }
			virtual int __stdcall Get234() { return 234; }
			virtual int __stdcall Get235() { return 235; }
			virtual int __stdcall Get236() { return 236; }
			virtual int __stdcall Get237() { return 237; }
			virtual int __stdcall Get238() { return 238; }
			virtual int __stdcall Get239() { return 239; }
			virtual int __stdcall Get240() { return 240; }
			virtual int __stdcall Get241() { return 241; }
			virtual int __stdcall Get242() { return 242; }
			virtual int __stdcall Get243() { return 243; }
			virtual int __stdcall Get244() { return 244; }
			virtual int __stdcall Get245() { return 245; }
			virtual int __stdcall Get246() { return 246; }
			virtual int __stdcall Get247() { return 247; }
			virtual int __stdcall Get248() { return 248; }
			virtual int __stdcall Get249() { return 249; }
			virtual int __stdcall Get250() { return 250; }
			virtual int __stdcall Get251() { return 251; }
			virtual int __stdcall Get252() { return 252; }
			virtual int __stdcall Get253() { return 253; }
			virtual int __stdcall Get254() { return 254; }
			virtual int __stdcall Get255() { return 255; }
			virtual int __stdcall Get256() { return 256; }
			virtual int __stdcall Get257() { return 257; }
			virtual int __stdcall Get258() { return 258; }
			virtual int __stdcall Get259() { return 259; }
			virtual int __stdcall Get260() { return 260; }
			virtual int __stdcall Get261() { return 261; }
			virtual int __stdcall Get262() { return 262; }
			virtual int __stdcall Get263() { return 263; }
			virtual int __stdcall Get264() { return 264; }
			virtual int __stdcall Get265() { return 265; }
			virtual int __stdcall Get266() { return 266; }
			virtual int __stdcall Get267() { return 267; }
			virtual int __stdcall Get268() { return 268; }
			virtual int __stdcall Get269() { return 269; }
			virtual int __stdcall Get270() { return 270; }
			virtual int __stdcall Get271() { return 271; }
			virtual int __stdcall Get272() { return 272; }
			virtual int __stdcall Get273() { return 273; }
			virtual int __stdcall Get274() { return 274; }
			virtual int __stdcall Get275() { return 275; }
			virtual int __stdcall Get276() { return 276; }
			virtual int __stdcall Get277() { return 277; }
			virtual int __stdcall Get278() { return 278; }
			virtual int __stdcall Get279() { return 279; }
			virtual int __stdcall Get280() { return 280; }
			virtual int __stdcall Get281() { return 281; }
			virtual int __stdcall Get282() { return 282; }
			virtual int __stdcall Get283() { return 283; }
			virtual int __stdcall Get284() { return 284; }
			virtual int __stdcall Get285() { return 285; }
			virtual int __stdcall Get286() { return 286; }
			virtual int __stdcall Get287() { return 287; }
			virtual int __stdcall Get288() { return 288; }
			virtual int __stdcall Get289() { return 289; }
			virtual int __stdcall Get290() { return 290; }
			virtual int __stdcall Get291() { return 291; }
			virtual int __stdcall Get292() { return 292; }
			virtual int __stdcall Get293() { return 293; }
			virtual int __stdcall Get294() { return 294; }
			virtual int __stdcall Get295() { return 295; }
			virtual int __stdcall Get296() { return 296; }
			virtual int __stdcall Get297() { return 297; }
			virtual int __stdcall Get298() { return 298; }
			virtual int __stdcall Get299() { return 299; }
			virtual int __stdcall Get300() { return 300; }
			virtual int __stdcall Get301() { return 301; }
			virtual int __stdcall Get302() { return 302; }
			virtual int __stdcall Get303() { return 303; }
			virtual int __stdcall Get304() { return 304; }
			virtual int __stdcall Get305() { return 305; }
			virtual int __stdcall Get306() { return 306; }
			virtual int __stdcall Get307() { return 307; }
			virtual int __stdcall Get308() { return 308; }
			virtual int __stdcall Get309() { return 309; }
			virtual int __stdcall Get310() { return 310; }
			virtual int __stdcall Get311() { return 311; }
			virtual int __stdcall Get312() { return 312; }
			virtual int __stdcall Get313() { return 313; }
			virtual int __stdcall Get314() { return 314; }
			virtual int __stdcall Get315() { return 315; }
			virtual int __stdcall Get316() { return 316; }
			virtual int __stdcall Get317() { return 317; }
			virtual int __stdcall Get318() { return 318; }
			virtual int __stdcall Get319() { return 319; }
			virtual int __stdcall Get320() { return 320; }
			virtual int __stdcall Get321() { return 321; }
			virtual int __stdcall Get322() { return 322; }
			virtual int __stdcall Get323() { return 323; }
			virtual int __stdcall Get324() { return 324; }
			virtual int __stdcall Get325() { return 325; }
			virtual int __stdcall Get326() { return 326; }
			virtual int __stdcall Get327() { return 327; }
			virtual int __stdcall Get328() { return 328; }
			virtual int __stdcall Get329() { return 329; }
			virtual int __stdcall Get330() { return 330; }
			virtual int __stdcall Get331() { return 331; }
			virtual int __stdcall Get332() { return 332; }
			virtual int __stdcall Get333() { return 333; }
			virtual int __stdcall Get334() { return 334; }
			virtual int __stdcall Get335() { return 335; }
			virtual int __stdcall Get336() { return 336; }
			virtual int __stdcall Get337() { return 337; }
			virtual int __stdcall Get338() { return 338; }
			virtual int __stdcall Get339() { return 339; }
			virtual int __stdcall Get340() { return 340; }
			virtual int __stdcall Get341() { return 341; }
			virtual int __stdcall Get342() { return 342; }
			virtual int __stdcall Get343() { return 343; }
			virtual int __stdcall Get344() { return 344; }
			virtual int __stdcall Get345() { return 345; }
			virtual int __stdcall Get346() { return 346; }
			virtual int __stdcall Get347() { return 347; }
			virtual int __stdcall Get348() { return 348; }
			virtual int __stdcall Get349() { return 349; }
			virtual int __stdcall Get350() { return 350; }
			virtual int __stdcall Get351() { return 351; }
			virtual int __stdcall Get352() { return 352; }
			virtual int __stdcall Get353() { return 353; }
			virtual int __stdcall Get354() { return 354; }
			virtual int __stdcall Get355() { return 355; }
			virtual int __stdcall Get356() { return 356; }
			virtual int __stdcall Get357() { return 357; }
			virtual int __stdcall Get358() { return 358; }
			virtual int __stdcall Get359() { return 359; }
			virtual int __stdcall Get360() { return 360; }
			virtual int __stdcall Get361() { return 361; }
			virtual int __stdcall Get362() { return 362; }
			virtual int __stdcall Get363() { return 363; }
			virtual int __stdcall Get364() { return 364; }
			virtual int __stdcall Get365() { return 365; }
			virtual int __stdcall Get366() { return 366; }
			virtual int __stdcall Get367() { return 367; }
			virtual int __stdcall Get368() { return 368; }
			virtual int __stdcall Get369() { return 369; }
			virtual int __stdcall Get370() { return 370; }
			virtual int __stdcall Get371() { return 371; }
			virtual int __stdcall Get372() { return 372; }
			virtual int __stdcall Get373() { return 373; }
			virtual int __stdcall Get374() { return 374; }
			virtual int __stdcall Get375() { return 375; }
			virtual int __stdcall Get376() { return 376; }
			virtual int __stdcall Get377() { return 377; }
			virtual int __stdcall Get378() { return 378; }
			virtual int __stdcall Get379() { return 379; }
			virtual int __stdcall Get380() { return 380; }
			virtual int __stdcall Get381() { return 381; }
			virtual int __stdcall Get382() { return 382; }
			virtual int __stdcall Get383() { return 383; }
			virtual int __stdcall Get384() { return 384; }
			virtual int __stdcall Get385() { return 385; }
			virtual int __stdcall Get386() { return 386; }
			virtual int __stdcall Get387() { return 387; }
			virtual int __stdcall Get388() { return 388; }
			virtual int __stdcall Get389() { return 389; }
			virtual int __stdcall Get390() { return 390; }
			virtual int __stdcall Get391() { return 391; }
			virtual int __stdcall Get392() { return 392; }
			virtual int __stdcall Get393() { return 393; }
			virtual int __stdcall Get394() { return 394; }
			virtual int __stdcall Get395() { return 395; }
			virtual int __stdcall Get396() { return 396; }
			virtual int __stdcall Get397() { return 397; }
			virtual int __stdcall Get398() { return 398; }
			virtual int __stdcall Get399() { return 399; }
			virtual int __stdcall Get400() { return 400; }
			virtual int __stdcall Get401() { return 401; }
			virtual int __stdcall Get402() { return 402; }
			virtual int __stdcall Get403() { return 403; }
			virtual int __stdcall Get404() { return 404; }
			virtual int __stdcall Get405() { return 405; }
			virtual int __stdcall Get406() { return 406; }
			virtual int __stdcall Get407() { return 407; }
			virtual int __stdcall Get408() { return 408; }
			virtual int __stdcall Get409() { return 409; }
			virtual int __stdcall Get410() { return 410; }
			virtual int __stdcall Get411() { return 411; }
			virtual int __stdcall Get412() { return 412; }
			virtual int __stdcall Get413() { return 413; }
			virtual int __stdcall Get414() { return 414; }
			virtual int __stdcall Get415() { return 415; }
			virtual int __stdcall Get416() { return 416; }
			virtual int __stdcall Get417() { return 417; }
			virtual int __stdcall Get418() { return 418; }
			virtual int __stdcall Get419() { return 419; }
			virtual int __stdcall Get420() { return 420; }
			virtual int __stdcall Get421() { return 421; }
			virtual int __stdcall Get422() { return 422; }
			virtual int __stdcall Get423() { return 423; }
			virtual int __stdcall Get424() { return 424; }
			virtual int __stdcall Get425() { return 425; }
			virtual int __stdcall Get426() { return 426; }
			virtual int __stdcall Get427() { return 427; }
			virtual int __stdcall Get428() { return 428; }
			virtual int __stdcall Get429() { return 429; }
			virtual int __stdcall Get430() { return 430; }
			virtual int __stdcall Get431() { return 431; }
			virtual int __stdcall Get432() { return 432; }
			virtual int __stdcall Get433() { return 433; }
			virtual int __stdcall Get434() { return 434; }
			virtual int __stdcall Get435() { return 435; }
			virtual int __stdcall Get436() { return 436; }
			virtual int __stdcall Get437() { return 437; }
			virtual int __stdcall Get438() { return 438; }
			virtual int __stdcall Get439() { return 439; }
			virtual int __stdcall Get440() { return 440; }
			virtual int __stdcall Get441() { return 441; }
			virtual int __stdcall Get442() { return 442; }
			virtual int __stdcall Get443() { return 443; }
			virtual int __stdcall Get444() { return 444; }
			virtual int __stdcall Get445() { return 445; }
			virtual int __stdcall Get446() { return 446; }
			virtual int __stdcall Get447() { return 447; }
			virtual int __stdcall Get448() { return 448; }
			virtual int __stdcall Get449() { return 449; }
			virtual int __stdcall Get450() { return 450; }
			virtual int __stdcall Get451() { return 451; }
			virtual int __stdcall Get452() { return 452; }
			virtual int __stdcall Get453() { return 453; }
			virtual int __stdcall Get454() { return 454; }
			virtual int __stdcall Get455() { return 455; }
			virtual int __stdcall Get456() { return 456; }
			virtual int __stdcall Get457() { return 457; }
			virtual int __stdcall Get458() { return 458; }
			virtual int __stdcall Get459() { return 459; }
			virtual int __stdcall Get460() { return 460; }
			virtual int __stdcall Get461() { return 461; }
			virtual int __stdcall Get462() { return 462; }
			virtual int __stdcall Get463() { return 463; }
			virtual int __stdcall Get464() { return 464; }
			virtual int __stdcall Get465() { return 465; }
			virtual int __stdcall Get466() { return 466; }
			virtual int __stdcall Get467() { return 467; }
			virtual int __stdcall Get468() { return 468; }
			virtual int __stdcall Get469() { return 469; }
			virtual int __stdcall Get470() { return 470; }
			virtual int __stdcall Get471() { return 471; }
			virtual int __stdcall Get472() { return 472; }
			virtual int __stdcall Get473() { return 473; }
			virtual int __stdcall Get474() { return 474; }
			virtual int __stdcall Get475() { return 475; }
			virtual int __stdcall Get476() { return 476; }
			virtual int __stdcall Get477() { return 477; }
			virtual int __stdcall Get478() { return 478; }
			virtual int __stdcall Get479() { return 479; }
			virtual int __stdcall Get480() { return 480; }
			virtual int __stdcall Get481() { return 481; }
			virtual int __stdcall Get482() { return 482; }
			virtual int __stdcall Get483() { return 483; }
			virtual int __stdcall Get484() { return 484; }
			virtual int __stdcall Get485() { return 485; }
			virtual int __stdcall Get486() { return 486; }
			virtual int __stdcall Get487() { return 487; }
			virtual int __stdcall Get488() { return 488; }
			virtual int __stdcall Get489() { return 489; }
			virtual int __stdcall Get490() { return 490; }
			virtual int __stdcall Get491() { return 491; }
			virtual int __stdcall Get492() { return 492; }
			virtual int __stdcall Get493() { return 493; }
			virtual int __stdcall Get494() { return 494; }
			virtual int __stdcall Get495() { return 495; }
			virtual int __stdcall Get496() { return 496; }
			virtual int __stdcall Get497() { return 497; }
			virtual int __stdcall Get498() { return 498; }
			virtual int __stdcall Get499() { return 499; }
		} _hooks_vtable;

		T* t = reinterpret_cast<T*>(&_hooks_vtable);
		typedef int (__stdcall T::* GetIndex)();
		GetIndex getIndex = (GetIndex)func;
		return (t->*getIndex)();
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (T::*func)(Args...), R (__fastcall *detour)(T*, Args...))
	{
		int vtableIndex = getVTableIndex(reinterpret_cast<R (T::*)(Args...)>(func));
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (T::*func)(Args...), R (__fastcall *detour)(T*, void*, Args...))
	{
		int vtableIndex = getVTableIndex(func);
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (T::*func)(Args...) const, R (__fastcall *detour)(T*, void*, Args...))
	{
		int vtableIndex = getVTableIndex(reinterpret_cast<R (T::*)(Args...)>(func));
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (T::*func)(Args...) const, R (__fastcall *detour)(T*, Args...))
	{
		int vtableIndex = getVTableIndex(reinterpret_cast<R (T::*)(Args...)>(func));
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}

	template <typename T, typename R, typename... Args>
	void InstallVirtualFunctionHook(const std::string &name, T* instance, R (__stdcall T::*func)(Args...), R (__stdcall *detour)(T*, Args...))
	{
		int vtableIndex = getVTableIndex(func);
		CryLogAlways("Found vtable index %i for hook %s", vtableIndex, name.c_str());
		InstallVirtualFunctionHook(name, (void*)instance, vtableIndex, (void*)detour);
	}
#endif
}
