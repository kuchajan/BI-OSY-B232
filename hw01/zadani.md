# Příhradová konstrukce

Úkolem je realizovat sadu tříd, které budou optimalizovat příhradové konstrukce.

Předpokládáme, že máme zkonstruovat nosník, který bude co nejpevnější a zároveň co nejlevnější. Nosník bude mít tvar mnohoúhelníku, může být obecně nekonvexní. Takový mnohoúhelník vyztužíme tím, že přidáme dodatečné výztuhy propojující body mnohoúhelníku. Aby byl nosník co nejpevnější, přidáme co nejvíce takových dodatečných výztuh, které se nekříží. Výztuhy tak rozdělí mnohoúhelník na disjunktní trojúhelníky. Úkolem je nalézt umístění výztuh tak, aby se spotřebovalo co nejméně materiálu, tedy aby součet délek dodaných výztuh byl co nejmenší. Formálně se jedná o problém minimální triangulace nekonvexního mnohoúhelníku.

Druhým úkolem je určit počet různých možností, kterými lze výztuhy umístit. Například pro obdélník jsou možnosti 2 - můžeme si vybrat jednu z úhlopříček. Pro větší počet stran mnohoúhelníku a složitější tvary je pak výpočet náročnější. Počet různých triangulací navíc poměrně rychle roste, proto je pro reprezentaci použit vlastní celočíselný datový typ o velikosti 1024 bitů.

Oba problémy lze řešit v polynomiálním čase se složitostí O(n3), proto bude pro zrychlení výpočtu vhodné zapojit více vláken. Očekávané řešení se musí správně zapojit do níže popsané infrastruktury a musí správně vytvářet, plánovat, synchronizovat a ukončovat vlákna. Vlastní algoritmické řešení problému není úplně nutné, testovací prostředí nabízí rozhraní, které dokáže zadaný problém sekvenčně vyřešit.

* * *

Problém k vyřešení je reprezentován instancí CPolygon. Třída obsahuje členské proměnné s umístěním jednotlivých bodů mnohoúhelníku a členské proměnné pro vypočtenou minimální triangulaci a počet triangulací.

Problémy jsou zabalené do skupin (balíků), takovou skupinu reprezentuje třída CProblemPack. V balíku jsou dva oddělené seznamy: v jednom seznamu jsou mnohoúhelníky, kde chceme spočítat minimální triangulaci, ve druhém pak mnohoúhelníky, kde máme spočítat počet triangulací.

Problém triangulace je zajímavý pro více firem, které se zabývají konstrukcemi. Firmy budou průběžně dodávat balíky problémů k vyřešení. Zadavatel je realizován třídou CCompany. Tato třída obsahuje rozhraní, které umožní načítání dalších balíků problémů k vyřešení (metoda waitForPack) a rozhraní, kterým se vrací vyřešené balíky problémů zpět (metoda solvedPack). Vaše implementace vytvoří 2 pomocná vlákna pro **každou** instanci CCompany. Jedno vlákno bude v cyklu načítat nové balíky problémů k vyřešení a předávat je dále ke zpracování. Druhé vlákno bude vyřešené balíky problémů vracet (bude v cyklu volat solvedPack). Tato pomocná vlákna zajišťují komunikaci s řešičem. Nejsou ale určena pro provádění vlastních výpočtů (řešené problémy jsou výpočetně náročné, komunikační vlákna by nemusela včas předat data). Úkolem komunikačních vláken je ale zjistit správné pořadí odevzdávání vyřešených balíků problémů, odevzdávat je potřeba ve stejném pořadí, ve kterém byly problémy načteny. V jednom okamžiku lze načíst více instancí balíků problémů a mít je rozpracované (je to dokonce žádoucí a nutné). Důležité je ale dodržet pořadí při jejich odevzdání.

Celý výpočet zapouzdřuje instance COptimizer. Tato třída obsahuje odkazy na jednotlivé firmy, řídí spouštění a pozastavení výpočtu a řídí činnost výpočetních vláken. Výpočetní vlákna jsou použita pro řešení zadávaných problémů a jsou použita pro řešení problémů ze všech obsluhovaných firem. Výpočetní vlákna převezmou problém od komunikačního vlákna, provedou vlastní výpočty a vyřešený problém předají zpět komunikačnímu vláknu (té správné firmy), aby jej toto ve vhodný okamžik předalo zpět (je potřeba dodržet pořadí). Výpočetních vláken bude existovat právě a pouze zadaný počet, to umožní vhodně využít hardwarové vybavení počítače (počet CPU).

Třída COptimizer bude použita podle následujícího scénáře:

*   vytvoří se instance COptimizer,
*   voláním metody addCompany se zaregistrují jednotlivé firmy,
*   spustí se vlastní výpočet (voláním metody start). Metoda start dostane v parametru počet výpočetních vláken (workThreads). Tato vlákna vytvoří a nechá je čekat na příchozí požadavky. Zároveň pro každou zaregistrovanou firmu vytvoří dvě komunikační vlákna, jedno pro příjem požadavků a druhé pro odevzdávání výsledků. Po spuštění vláken se metoda start vrátí zpět do volajícího,
*   komunikační vlákna průběžně přebírají požadavky (v cyklu volají odpovídající metody waitForPack). Vlákno přebírající požadavky se ukončí v okamžiku, kdy načtete prázdný požadavek (smart pointer obsahuje nullptr),
*   výpočetní vlákna přebírají problémy od komunikačních vláken a řeší je. Po vyřešení problém předají odevzdávacímu vláknu odpovídající firmy,
*   odevzdávací vlákna čekají na vyřešené problémy a vyhodnocují, které balíky problémů jsou již zcela vyřešené. Po vyřešení celého balíku problémů bude tento balík vrácen zadávající firmě (metoda solvedPack). Vyřešené balíky problémů musíte odevzdat okamžitě, jak to je (vzhledem k pořadí) možné (vyřešené balíky problémů nelze ukládat a odevzdat až na konci výpočtu). Odevzdávací vlákno skončí, pokud již žádné další problémy z dané firmy nepřijdou (waitForPack dříve vrátil nullptr) a všechny balíky problémů této firmy byly vyřešené a odevzdané,
*   testovací prostředí v nějakém okamžiku zavolá metodu stop. Volání počká na dokončení obsluhy všech firem, ukončí pracovní vlákna a vrátí se do volajícího,
*   je uvolněna instance COptimizer.

Použité třídy a jejich rozhraní:

*   CPoint je třída reprezentující jeden bod mnohoúhelníku. Jedná se o velmi jednoduchou třídu, která zapouzdřuje dvojici celých čísel - souřadnice. Třída je implementovaná v testovacím prostředí, její rozhraní nesmíte nijak měnit. Rozhraní obsahuje:
    *   m\_X souřadnice x,
    *   m\_Y souřadnice y.
*   CPolygon je třída reprezentující jeden mnohoúhelník. Jedná se o abstraktní třídu, její implementace (přesněji, implementace potomka) je hotová v testovacím prostředí. Rozhraní nesmíte nijak měnit. Rozhraní obsahuje:
    *   m\_Points vektor souřadnic s vrcholy mnohoúhelníku,
    *   m\_TriangMin členská proměnná, kam bude uložena vypočtená délka minimální triangulace. Uložena hodnota bude zahrnovat součet délek stran mnohoúhelníku plus součet délek dodaných uhlopříček.
    *   m\_TriangCnt členská proměnná, kam bude uložen vypočtený počet různých triangulací daného mnohoúhelníku.
    *   třída dále obsahuje konstruktor a metodu add pro zjednodušení práce (viz přiložená implementace).
*   CProblemPack je třída reprezentující balík problémů k vyřešení. Jedná se o abstraktní třídu, její implementace (přesněji, implementace potomka) je hotová v testovacím prostředí. Rozhraní nesmíte nijak měnit. Rozhraní obsahuje:
    *   m\_ProblemsMin pole instancí problémů k vyřešení, požaduje se výpočet minimální triangulace,
    *   m\_ProblemsCnt pole instancí problémů k vyřešení, požaduje se výpočet počtu triangulací,
    *   třída dále obsahuje metodu addMin a addCnt pro zjednodušení práce (viz přiložená implementace).
*   CCompany je třída reprezentující jednu firmu. Třída pouze definuje rozhraní, faktická implementace v testovacím prostředí je potomkem CCompany. Rozhraní třídy tedy nemůžete měnit. K dispozici jsou metody:
    *   waitForPack pro načtení dalšího balíku problémů z firmy. Metoda vrátí instanci ke zpracování nebo neplatný ukazatel (smart pointer obsahuje nullptr), pokud již nejsou pro tuto firmu další instance balíků problémů ke zpracování. Volání metody může trvat dlouho, proto pro obsluhu musíte vyčlenit oddělené komunikační vlákno, které bude metodu v cyklu volat. Vlákno nesmí provádět žádnou výpočetně náročnou činnost, musí získanou instanci CProblemPack předat ke zpracování pracovním vláknům. Kontroluje se, že v jedné instanci firmy volá tuto metodu vždy jedno (stejné) vlákno,
    *   solvedPack pro předání vyřešené instance CProblemPack. Parametrem je vyřešená instance balíku problému dříve získaná z volání waitForPack. Protože odevzdání může trvat dlouho, musíte pro odevzdávání vytvořit vyhrazené komunikační vlákno. Vlákno bude přebírat od pracovních vláken vyřešené instance problémů, rozhodne, které balíky problémů jsou zcela vyřešené a zavolá na ně metodu solvedPack. Vyřešené instance balíků problémů musí být vracené ve stejném pořadí, ve kterém byly z waitForPack převzaté. Předávací vlákno nesmí provádět žádnou výpočetně náročnou činnost. Kontroluje se, že v jedné instanci firmy volá tuto metodu vždy jedno (stejné) vlákno.
*   CBigInt je třída implementující velká celá kladná čísla. Čísla jsou reprezentovaná binárně pomocí 1024 bitů. Třída je implementovaná jednak v testovacím prostředí a dále je k dispozici i v dodané knihovně. Implementace je omezena pouze na základní operace: nastavení hodnoty (z uint64\_t a z desítkového zápisu v řetězci), převod hodnoty na řetězec v desítkovém zápisu, sčítání, násobení a porovnání. Ostatní operace nejsou k dispozici a nejsou pro implementaci potřeba.
*   COptimizer je třída zapouzdřující celý výpočet. Třídu budete vytvářet vy, musíte ale dodržet následující veřejné rozhraní:
    *   konstruktor bez parametrů inicializuje novou instanci třídy. Zatím nevytváří žádná vlákna,
    *   metodu addCompany (x), tato metoda zaregistruje firmu x,
    *   metodu start ( workThr ), tato metoda vytvoří komunikační vlákna pro všechny zaregistrované firmy a spustí workThr pracovních vláken. Po spuštění vláken se metoda start vrátí zpět do volajícího,
    *   metodu stop, která počká na dokončení obsluhy firem a ukončení vytvořených vláken. Po tomto se volání stop vrátí zpět do volajícího,
    *   metoda usingProgtestSolver() vrátí hodnotu true, pokud pro vlastní řešení problémů používáte dodaný řešič CProgtestSolver nebo hodnotu false pokud celý výpočet implementujete vlastním kódem. Pokud tato metoda vrací true, testovací prostředí nepoužívá metody COptimizer::checkAlgorithmMin() a COptimizer::checkAlgorithmCnt() níže (můžete je ponechat prázdné). Pokud metoda vrací false, testovací prostředí upraví chování poskytovaného řešiče CProgtestSolver tak, že úmyslně vyplňuje nesprávné výsledky.
    *   třídní metodu checkAlgorithmMin(polygon). Metoda slouží k otestování správnosti vlastního algoritmu výpočtu. Parametrem volání je jedna instance CPolygon. Kód metody zajistí potřebné výpočty a vyplní složku m\_TriangMin v instanci polygon. Kromě kontroly správnosti implementovaných algoritmů se metoda používá ke kalibraci rychlosti vašeho řešení. Rychlosti se přizpůsobí velikost zadávaných problémů, aby testování trvalo rozumně dlouhou dobu. Metodu implementujte pouze pokud nepoužíváte dodaný řešič problémů CProgtestSolver (tedy pokud Vaše metoda COptimizer::usingProgtestSolver() vrací false),
    *   třídní metodu checkAlgorithmCnt(polygon). Metoda slouží k otestování správnosti vlastního algoritmu výpočtu. Parametrem volání je jedna instance CPolygon. Kód metody zajistí potřebné výpočty a vyplní složku m\_TriangCnt v instanci polygon. Kromě kontroly správnosti implementovaných algoritmů se metoda používá ke kalibraci rychlosti vašeho řešení. Rychlosti se přizpůsobí velikost zadávaných problémů, aby testování trvalo rozumně dlouhou dobu. Metodu implementujte pouze pokud nepoužíváte dodaný řešič problémů CProgtestSolver (tedy pokud Vaše metoda COptimizer::usingProgtestSolver() vrací false),
*   CProgtestSolver je třída poskytující rozhraní pro sekvenční řešení zadávaných problémů. Její autor se rozhodl trochu zpestřit její používání, proto je chování této třídy poněkud rozverné. Třída CProgtestSolver je abstraktní, vlastní implementace je realizovaná podtřídami. Instance potomků CProgtestSolver se vytvářejí pomocí factory funkcí createProgtestMinSolver() (umí řešit problémy minimální triangulace) a createProgtestCntSolver() (umí řešit problémy nalezení počtu triangulací). Instance CProgtestSolver navíc řeší zadávané problémy dávkově. Každá instance CProgtestSolver má určenou kapacitu, kolik instancí problému do ní lze nejvýše umístit. Instance CProgtestSolver se používá pouze jednorázově. Pokud je potřeba vyřešit další instance problémů, je potřeba vyrobit další instanci CProgtestSolver. Třída má následující rozhraní:
    
    *   hasFreeCapacity() metoda vrátí true, pokud lze do instance umístit další instanci problému k vyřešení, false, pokud je instance zcela zaplněná,
    *   addPolygon(x) metoda přidá instanci problému x k vyřešení. Návratovou hodnotou je true pokud byl problém přidán, false pokud ne (protože byla dosažena kapacita řešiče). Po vložení problému je rozumné otestovat, zda je kapacita řešiče zaplněná (hasFreeCapacity). Pokud je kapacita využitá, spusťte výpočet (solve).
    *   solve() metoda spustí vlastní výpočet. Pro vložené instance se určí výsledek a umístí se do odpovídající složky m\_TriangMin / m\_TriangCnt. Metoda nedělá nic dalšího, zejména se nepokouší informovat o vyřešení zadaných problémů (nevolá CCompany::solvedPack). Další zpracování vyřešených problémů je čistě Vaše zodpovědnost. Metodu solve lze zavolat pro danou instanci CProgtestSolver pouze jednou, další pokusy skončí chybou. Metoda vrací počet vyřešených instancí problémů, návratová hodnota 0 typicky znamená chybu (opakovaný pokus o volání metody).
    
    Instance CProgtestSolver nedovolí vložit více problémů než je její kapacita. Na druhou stranu, metodu solve lze zavolat kdykoliv (ale jen jednou pro danou instanci). Nepokoušejte se ale řešič zneužívat a řešit problémy pouze po jednom:
    
    *   testovací prostředí vytváří instance CProgtestSolver tak, že součet jejich kapacit M pokrývá celkový počet zadávaných problémů N v testu (typicky M je o něco málo větší než N),
    *   pokud byste každou instanci CProgtestSolver využili pouze k vyřešení jedné instance problému, brzy vyčerpáte kapacitu M a další zadávané problémy nebudete mít jak řešit,
    *   pokud překročíte kapacitu M, budou volání createProgtestMinSolver() / createProgtestCntSolver() vracet neužitečné instance řešičů (podle nálady bude vrácen prázdný smart pointer, řešič bude mít kapacitu 0 nebo bude úmyslně vyplňovat nesprávné výsledky),
    *   proto je důležité kapacitu řešičů využívat naplno,
    *   kapacity jednotlivých instancí řešičů jsou volené náhodně. Jak již bylo řečeno, tato třída se snaží zpestřit programátorům nudnou práci.
    
    Dále, testovacím prostředím poskytovaný řešič je k dispozici pouze v povinných a nepovinných testech (není k dispozici v bonusových testech). Pokud se jej v bonusovém testu pokusíte použít, bude factory funkce createProgtestMinSolver() / createProgtestCntSolver() vracet prázdné instance, případně instance s nulovou kapacitou či počítající nesprávné výsledky.
    
*   createProgtestMinSolver - funkce vytvoří instanci CProgtestSolver, která bude řešit problémy minimální triangulace.
*   createProgtestCntSolver - funkce vytvoří instanci CProgtestSolver, která bude řešit problémy nalezení počtu triangulací.

* * *

Odevzdávejte zdrojový kód s implementací požadované třídy COptimizer s požadovanými metodami. Můžete samozřejmě přidat i další podpůrné třídy a funkce. Do Vaší implementace nevkládejte funkci main ani direktivy pro vkládání hlavičkových souborů. Funkci main a hlavičkové soubory lze ponechat pouze v případě, že jsou zabalené v bloku podmíněného překladu.

Využijte přiložený ukázkový soubor. Celá implementace patří do souboru solution.cpp. Pokud zachováte bloky podmíněného překladu, můžete soubor solution.cpp odevzdávat jako řešení úlohy.

Při řešení lze využít pthread nebo C++11 API pro práci s vlákny (viz vložené hlavičkové soubory). Dostupný je kompilátor g++ verze 12.2, tato verze kompilátoru zvládá většinu C++11, C++14, C++17 a C++20 konstrukcí (některé okrajové vlastnosti C++20 nejsou podporované plně).

* * *

**Doporučení:**  

*   Začněte se rovnou zabývat vlákny a synchronizací, nemusíte se zabývat vlastními algoritmy řešení zadaných problémů. Využijte dodané řešení - třídu CProgtestSolver. Až budete mít hotovou synchronizaci, můžete doplnit i vlastní algoritmické řešení.
*   Abyste zapojili co nejvíce jader, obsluhujte co nejvíce instancí CPolygon najednou. Je potřeba zároveň přebírat problémy, řešit je a odevzdávat je. Nepokoušejte se tyto činnosti nafázovat (například nejdříve pouze počkat na všechny CProblemPack, pak začít řešit akumulované problémy, ...), takový postup nebude fungovat. Testovací prostředí je nastaveno tak, aby takové "serializované" řešení vedlo k uváznutí (deadlock).
*   Instance COptimizer je vytvářená opakovaně, pro různé vstupy. Nespoléhejte se na inicializaci globálních proměnných - při druhém a dalším zavolání budou mít globální proměnné hodnotu jinou. Je rozumné případné globální proměnné vždy inicializovat v konstruktoru nebo na začátku metody start. Ještě lepší je nepoužívat globální proměnné vůbec.
*   Nepoužívejte mutexy a podmíněné proměnné inicializované pomocí PTHREAD\_MUTEX\_INITIALIZER, důvod je stejný jako v minulém odstavci. Použijte raději pthread\_mutex\_init() nebo C++11 API.
*   Instance problémů (CPolygon), balíků problémů (CProblemPack), firem (CCompany) a řešičů (CProgtestSolver) alokovalo testovací prostředí při vytváření příslušných smart pointerů. K uvolnění dojde automaticky po zrušení všech odkazů. Uvolnění těchto instancí tedy není Vaší starostí, stačí zapomenout všechny takto předané smart pointery. Váš program je ale zodpovědný za uvolnění všech ostatních prostředků, které si sám alokoval.
*   Neukončujte metodu stop pomocí exit, pthread\_exit a podobných funkcí. Pokud se funkce stop nevrátí do volajícího, bude Vaše implementace vyhodnocena jako nesprávná.
*   Využijte přiložená vzorová data. V archivu jednak naleznete ukázku volání rozhraní a dále několik testovacích vstupů a odpovídajících výsledků.
*   V testovacím prostředí je k dispozici STL. Myslete ale na to, že ten samý STL kontejner nelze najednou zpřístupnit z více vláken. Více si o omezeních přečtěte např. na [C++ reference - thread safety.](http://en.cppreference.com/w/cpp/container)
*   Testovací prostředí je omezené velikostí paměti. Není uplatňován žádný explicitní limit, ale VM, ve které testy běží, je omezena 4 GiB celkové dostupné RAM. Úloha může být dost paměťově náročná, zejména pokud se rozhodnete pro jemné členění úlohy na jednotlivá vlákna. Pokud se rozhodnete pro takové jemné rozčlenění úlohy, možná budete muset přidat synchronizaci běhu vláken tak, aby celková potřebná paměť v žádný okamžik nepřesáhla rozumný limit. Pro běh máte garantováno, že Váš program má k dispozici nejméně 1 GiB pro Vaše data (data segment + stack + heap). Pro zvídavé - zbytek do 4GiB je zabraný běžícím OS, dalšími procesy, zásobníky Vašich vláken a nějakou rezervou.
*   Výpočetně náročné operace musí provádět pracovní vlákna. Počet pracovních vláken je určen parametrem metody COptimizer::start. Testovací prostředí kontroluje, zda Vaše implementace neprovádí výpočetně náročné operace v ostatních vláknech. Pokud to zjistí, Vaše řešení bude odmítnuto.
*   Explicitní nastavení počtu pracovních vláken má dobré praktické důvody. Volbou rozumného počtu pracovních vláken můžeme systém zatížit dle naší volby (tedy například podle počtu jader, která můžeme úloze přidělit). Pokud by časově náročné výpočty probíhaly i v jiných vláknech (komunikační vlákna), pak by bylo možné systém snadno zahltit například při velkém množství registrovaných firem.

**Doporučení pro implementaci vlastního algoritmického řešení:**  

*   Vlastní algoritmické řešení není nutné, je k dispozici dodaný CProgtestSolver. Použité algoritmy nejsou obtížné, ale můžete narazit na nepříjemné problémy z analytické geometrie (průsečíky úseček, zaokrouhlovací chyby, neceločíselná aritmetika).
*   CProgtestSolver se snaží vyhnout výpočtům v plovoucí desetinné čárce, zejména při výpočtu průsečíků úseček. To se daří díky tomu, že zadané souřadnice jsou celá čísla typu int a při výpočtu stačí pracovat se zlomky, kde čitatel i jmenovatel má typ int (rovnice lze následně přenásobit jmenovatelem a pracovat v typu long int). Desetinná čísla jsou použitá pouze pro finální výpočet délky triangulace.
*   Zadávané mnohoúhelníky mohou být konvexní i nekonvexní. Pro nekonvexní mnohoúhelníky jsou algoritmy často komplikovanější a časově náročnější. Testovací prostředí zadává pouze "hezké" nekonvexní mnohoúhelníky: zadávané mnohoúhelníky neobsahují "díry" a strany zadávaných mnohoúhelníků se neprotínají (simple polygon). Díky tomuto omezení vychází algoritmy ještě zvládnutelně.
*   Algoritmus minimální triangulace konvexních polygonů je oblíbený problém pro aplikaci dynamického programování. Rozšíření pro nekonvexní polygony je přímočaré, jen je potřeba kontrolovat možnost dělení polygonů v daném místě.
*   Výpočet celkového počtu triangulací je opět aplikací dynamického programování. Pro konvexní polygony je pro výpočet k dispozici explicitní vzorec - počet triangulací je Catalaniho číslo. Výpočet pro nekonvexní polygon vychází z myšlenky odvození vzorce pro konvexní polygony, jen je potřeba eliminovat některá dělení, která v nekonvexním polygonu nejsou přípustná.
*   Vaše implementace řešiče musí mít rozumnou složitost. Implementované řešení má časovou složitost O(n3) a paměťovou složitost O(n2), tyto složitosti platí pro oba řešené problémy. Pro výpočet počtu triangulací se ale dá očekávat výrazně vyšší multiplikativní konstanta (práce s velkými čísly).
*   Pokud správně implementujete vlastní řešení se stejnými složitostmi, vyhoví pro kompletní vyřešení této úlohy. Jak již bylo uvedeno výše, testovací prostředí kalibruje rychlost použitého algoritmu a naměřenou rychlost používá pro nastavení velikosti řešených problémů. Díky tomu si v určitých mezích dokáže poradit s různě efektivními implementacemi.
*   Pro úspěšné zvládnutí bonusu #2 a #3 je potřeba zapojit více vláken i do výpočtu triangulace jednotlivých polygonů. Polygony zadávané v těchto testech mají typicky několik stovek vrcholů.

* * *

**Co znamenají jednotlivé testy:**  

**Test algoritmu (sekvencni)**

Testovací prostředí opakovaně volá metody checkAlgorithmMin() / checkAlgorithmCnt() pro různé vstupy a kontroluje vypočtené výsledky. Slouží pro otestování implementace Vašeho algoritmu. Není vytvářena instance COptimizer a není volaná metoda start. Na tomto testu můžete ověřit, zda Vaše implementace algoritmu je dostatečně rychlá. Testují se náhodně generované problémy, nejedná se o data z dodané ukázky. Pokud COptimizer::usingProgtestSolver vrací true, tento test se efektivně přeskočí.

**Základní test**

Testovací prostředí vytváří instanci COptimizer pro různý počet firem a pracovních vláken. Ve jménu testu je pak uvedeno, kolik je firem (C=xxx) a pracovních vláken (W=xxx).

**Základní test (W=n, C=m)**

V tomto testu se navíc kontroluje, zda průběžně odevzdáváte již vyřešené instance balíků problémů. Testovací prostředí v jednom okamžiku přestane dodávat nové balíky problémy, dokud nejsou všechny dosud zadané balíky problémů vyřešené a vrácené. Pokud nepřebíráte/neřešíte/neodevzdáváte balíky problémů průběžně, skončíte v tomto testu deadlockem.

Pokud používáte CProgtestSolver, vždy využívejte plně jeho kapacitu. V tomto testu jde požadavek na kompletní využití kapacity instance CProgtestSolver proti požadavku na průběžné řešení a odevzdávání vyřešených balíků problémů. Testovací prostředí v tomto testu dodává instance CProgtestSolver vždy s kapacitou 1, tedy **nemůže** nastat situace, kdy čekáte na další balík problémů (aby se využila kapacita řešiče) a zároveň testovací prostředí čeká na odevzdání zadaných balíků problémů (aby pokračovalo v zadávání).

**Test zrychleni vypoctu**

Testovací prostředí spouští Vaši implementaci pro ta samá vstupní data s různým počtem pracovních vláken. Měří se čas běhu (real i CPU). S rostoucím počtem vláken by měl real time klesat, CPU time mírně růst (vlákna mají možnost běžet na dalších CPU). Pokud real time neklesne, nebo klesne málo (např. pro 2 vlákna by měl ideálně klesnout na 0.5, je ponechaná určitá rezerva), test není splněn.

**Busy waiting (pomale waitForPack)**

Nové balíky problémů jsou předávané pomalu. Výpočetní vlákna tím nemají práci. Pokud tato vlákna nejsou synchronizovaná blokujícím způsobem, výrazně vzroste CPU time a test selže.

**Busy waiting (pomale solvedPack)**

Odevzdávání vyřešených balíků problémů je pomalé. Vyřešené problémy nemá od výpočetních vláken kdo přebírat. Pokud tato vlákna nejsou synchronizovaná blokujícím způsobem, výrazně vzroste CPU time a test selže.

**Rozlozeni zateze #1**

Testovací prostředí zkouší, zda se do řešení jedné instance CProblemPack dokáže zapojit více pracovních vláken. V instanci je více zadání problémů, tato zadání mohou být řešena nezávisle více pracovními vlákny. V testu není k dispozici CProgtestSolver.

**Rozlozeni zateze #2**

Testovací prostředí zkouší, zda se do řešení jedné instance problému minimální triangulace dokáže zapojit více pracovních vláken. V instanci CProblemPack je zadán pouze jeden polygon s velkým počtem vrcholů, do jeho řešení je potřeba zapojit více (všechna) pracovní vlákna. V testu není k dispozici CProgtestSolver.

**Rozlozeni zateze #3**

Testovací prostředí zkouší, zda se do řešení jedné instance problému zjištění počtu triangulací dokáže zapojit více pracovních vláken. V instanci CProblemPack je zadán pouze jeden problém s velkým počtem vrcholů, do jeho řešení je potřeba zapojit více (všechna) pracovní vlákna. V testu není k dispozici CProgtestSolver.

**Update 9.3.2024:**

*   metoda pro přidání problému do solveru se jmenuje CProgtestSolver::addPolygon (nesprávně bylo CProgtestSolver::addProblem),
*   v textu jsem na několika místech nahradil **problém** za **balík problémů**, aby popis v textu lépe odpovídal rozhraní tříd.