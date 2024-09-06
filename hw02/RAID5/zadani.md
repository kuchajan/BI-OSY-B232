Cílem této práce je podrobněji se seznámit s problematikou fungování řadičů RAID. Vaším úkolem je implementovat SW pro RAID 5. SW, který vyvinete, bude poskytovat funkce, tyto funkce realizují operace s RAID 5 svazkem jako s blokovým zařízením (tedy zejména zápis a čtení sektoru). Vlastní ukládání dat budete realizovat voláním poskytnutých funkcí, které budou simulovat zápis / čtení dat na jednotlivé disky.

Vaše implementace musí splňovat vlastnosti RAID 5 zařízení, tedy zejména:

    práce s n zařízeními, kde n >= 3,
    odolnost vůči výpadku jednoho zařízení,
    rovnoměrné rozkládání I/O operací na jednotlivé disky (tedy mj. parita nesmí být koncentrovaná na jednom fyzickém disku),
    kapacita vzniklého RAID 5 zařízení musí být n-1 kapacity jednoho zařízení (implementace může mít malý overhead, viz níže).

Celá implementace je realizovaná třídou CRaidVolume a sadou podpůrných deklarací:

SECTOR_SIZE
    udává velikost jednoho sektoru v bajtech. Sektor je pro blokové zařízení (disky, RAID) jednotkou pro operace, číst/zapisovat lze pouze celé sektory.
MAX_RAID_DEVICES
    udává max. počet zařízení, ze kterých bude cílový RAID 5 sestaven. Skutečný počet zařízení nikdy nepřekročí tuto mez.
MAX_DEVICE_SECTORS, MIN_DEVICE_SECTORS
    udává minimální a maximální počet sektorů na jednom disku. Skutečná kapacita jednoho disku bude vždy v těchto mezích.
RAID_OK
    označuje stav RAID zařízení kdy zařízení pracuje zcela správně.
RAID_DEGRADED
    označuje stav RAID zařízení kdy zařízení pracuje, ale jeden disk havaroval (tedy výpadek dalšího disku je již kritický).
RAID_FAILED
    označuje stav RAID zařízení kdy zařízení nepracuje, protože havarovaly dva nebo více disků.
RAID_STOPPED
    označuje stav RAID zařízení kdy zařízení nepracuje, protože RAID nebyl ještě sestaven (před zavoláním start nebo byl pozastaven (po zavolání stop).
TBlkDev
    je struktura, která je použitá pro I/O operace s jednotlivými disky, které tvoří RAID zařízení. Složka m_Devices udává kolik disků je v RAID zařízení celkem použito (3 až MAX_RAID_DEVICES). Složka m_Sectors udává celkový počet sektorů na každém jednom disku. Konečně, složky m_Read a m_Write jsou ukazatele na funkce, které budete volat při vlastní I/O operaci. Operací je buď čtení sektorů z disku (m_Read), nebo zápis na disk (m_Write). Najednou lze číst/zapisovat jeden sektor nebo více sousedních sektorů. Význam parametrů pro obě volání je stejný:

         TBlkDev * dev;
         ...
         ret = dev -> m_Read ( diskNr, secNr, data, secCnt );
       

    diskNr
        je číslo disku, kterého se operace týká. Číslo je z rozsahu 0 až m_Devices - 1.
    secNr
        udává číslo prvního čteného/zapisovaného sektoru. Čtené/zapisované sektory musí být v rozsahu 0 až m_Sectors - 1.
    data
        udává paměťový blok, kam budou data z disku načtena (operace čtení) resp. data, která budou zapsaná na disk. Paměťový blok musí mít velikost dostatečnou pro čtený/zapisovaný počet sektorů (viz níže).
    secCnt
        určuje počet čtených resp. zapisovaných sektorů. Hodnota 1 znamená zápis jednoho sektoru (tedy efektivně 512 B), ... Zápis více sousedních sektorů najednou snižuje režii, tedy urychluje I/O operace.
    návratová hodnota
        určuje počet sektorů, který byl skutečně načten/zapsán. Hodnota nižší než secCnt znamená buď pokus o čtení/zápis za posledním sektorem disku, nebo chybu zařízení.

CRaidVolume::create
    Volání této funkce zapíše na disky režijní informace, které pro Vaší implementaci RAID potřebujete. Tato metoda je volána při prvotním vytváření RAID zařízení (po instalaci nových disků (ekvivalent příkazu mdadm --create ... v Linuxu). Reálný RAID při inicializaci dopočte paritu a poznamenává si režijní bloky používané při sestavování RAID. Režijní blok např. obsahuje informace, které disky byly funkční při posledním ukončení RAID zařízení (např. aby poznal off-line výměnu nefunkčního disku). Vaše implementace je zjednodušena tím, že nové disky jsou vyplněné samými nulami, tedy prvotní synchronizaci nemusíte provádět (zde, v create). Samotné volání create neinicializuje RAID zařízení (tedy nevytváří instanci CRaidVolume). Parametrem je struktura pro komunikaci s disky. Návratovou hodnotou funkce je hodnota false pro neúspěch, true pro úspěch.
CRaidVolume::CRaidVolume
    Konstruktor inicializuje instanci RAID zařízení. Samotný konstruktor pouze nastaví členské proměnné na výchozí hodnoty, nebude se zabývat načítáním dat z vlastních disků (k diskům nemá zatím přístup, konstruktor nedostal TBlkDev). Zařízení je po inicializaci ve stavu RAID_STOPPED.
CRaidVolume::start
    Toto volání inicializuje nově vytvořený nebo dříve pozastavený RAID. Funkce načte Vaše režijní struktury z disků a připraví RAID zařízení pro čtecí/zápisové požadavky. RAID zařízení bude podle výsledku operace ve stavu RAID_OK, RAID_DEGRADED nebo RAID_FAILED. Tato operace neprovádí žádné pokusy o opravu obsahu (pokud např. jeden disk byl nahrazen). Operace hrubě odpovídá volání mdadm --assemble ... v Linuxu. Parametrem volání je struktura pro komunikaci s disky, návratovou hodnotou je stav RAID zařízení (RAID_OK, ...).
CRaidVolume::stop
    Toto volání uloží všechna neuložená data na disky, zapíše Vaše případné režijní informace a převede RAID zařízení do stavu RAID_STOPPED. Operace odpovídá volání mdadm --stop v Linuxu. Návratovou hodnotou by mělo být RAID_STOPPED.
CRaidVolume::status
    Metoda vrátí aktuální stav RAID zařízení, tedy jednu z konstant RAID_OK, RAID_DEGRADED, RAID_FAILED nebo RAID_STOPPED.
CRaidVolume::size
    Metoda vrátí počet dostupných sektorů na RAID zařízení. V ideálním případě by počet sektorů měl být (počet_disků - 1) * sektorů_na_disk. Vaše implementace ale může vyžadovat nějaké režijní informace, které si na jednotlivé disky uložíte (např. do prvních nebo do posledních sektorů disku). Touto funkcí tedy vracíte skutečně dostupnou kapacitu, která zbude po odpočtení Vaší režie. Zápisy a čtení na RAID zařízení budou probíhat pouze pro sektory v rozsahu 0 až size()-1. Vaše režie musí být rozumná (nesmí přesáhnout 1% celkové kapacity). Referenční řešení alokuje 1 sektor na každém disku.
CRaidVolume::read, CRaidVolume::write
    Metody provádějí vlastní čtení/zápis sektorů na RAID zařízení. Význam parametrů je pro obě volání stejný: sector udává číslo prvního čteného/zapisovaného sektoru (v rozsahu 0 až size()-1), data určují paměťový blok, kam mají být data z RAID zařízení načtena/která mají být uložena a sectorCnt určuje počet čtených/zapisovaných sektorů. Funkce vrátí hodnotu true (data byla úspěšně přečtena/zapsána) nebo false (data nebyla přečtena/zapsána). Pokud je RAID zařízení ve stavu RAID_DEGRADED, funkce hlásí úspěch (je povinností Vaší implementace data správně dopočítat při načítání/ukládání).
CRaidVolume::resync
    Pokud byl RAID ve stavu RAID_DEGRADED, funkce dopočte obsah na disku, který byl nefunkční. Pokud dopočtení parity uspěje, přejde RAID do stavu RAID_OK. Pokud při dopočítávání parity vypadne ten samý disk, zůstane RAID zařízení ve stavu RAID_DEGRADED. Pokud by ale při dopočítávání parity vypadl jiný než dříve nefunkční disk, přešel by RAID do stavu RAID_FAILED. Návratová hodnota je stav RAID zařízení (RAID_OK, RAID_DEGRADED, ...). Pozor! nahrazený disk může být buď nový (pak je vyplněn nulami) nebo se může jednat o disk, který se "vzpamatoval", tedy začal fungovat a má "nějaký náhodný obsah". Obsahu dříve nefunkčního disku tedy nemůžete věřit. Nemůžete věřit ani svým případným servisním záznamům, které jste si dříve na tento disk poznamenali. (Musíte se orientovat podle servisních záznamů na ostatních discích, bude asi vhodné servisní záznamy nějak verzovat. Např. číslem, které se bude s každým úspěšným start/stop zvětšovat.)

Odevzdávejte zdrojový kód s implementací požadované třídy a s případnými dalšími podpůrnými funkcemi, které Vaše implementace potřebuje. Deklaraci struktury TBlkDev, definice konstant a vkládání hlavičkových souborů ponechte v bloku podmíněného překladu. Do Vaší implementace nevkládejte funkci main ani direktivy pro vkládání hlavičkových souborů. Funkci main a hlavičkové soubory lze ponechat pouze v případě, že jsou zabalené v bloku podmíněného překladu. Při implementaci nepoužívejte STL.
Nápověda:

    Uložte si kopii struktury TBlkDev, neukládejte si pouze odkaz.
    Ve Vaší implementaci si můžete alokovat paměť (celkem až řádově stovky KiB), součet kapacity disků ale bude několikrát větší než dostupná velikost paměti. Nepokoušejte se řešení oblafnout tím, že byste data ukládali do alokované paměti místo na poskytnuté disky.
    Nezapomeňte, že veškeré I/O operace probíhají s celými sektory (tedy s 512 B). Tedy např. pokud budete načítat 2 sektory najednou, musíte připravit paměťový blok velikosti 1024 B.
    Zápis několika sektorů najednou může I/O operace zrychlit. Počet najednou zapisovaných sektorů ale není rozumné přehánět (i kvůli omezení paměťových alokací).
    Testovací prostředí si vede statistiku počtu sektorů čtených/zapisovaných z jednotlivých disků. RAID 5 by měl disky vytěžovat rovnoměrně, i při výpadku jednoho z disků. Pokud paritu umístíte na jeden dedikovaný disk, rovnoměrného počtu přístupů nedosáhnete.
    Využijte přiložený ukázkový soubor. V souboru je implementován jednoduchý diskový subsystém, který můžete použít jako základ pro Vaše testování. Pro důkladné otestování bude potřeba dodaný soubor rozšířit, zejména je potřeba implementovat mnohem důkladnější testování obsahu, otestovat odolnosti vůči výpadku a testování obnovy RAID zařízení.
    Vaše implementace je testována pouze jedním vláknem, sekvenčně. Nezabývejte se zamykáním.
    Pokud funkce pro čtení/zapisování dat na disk jednou selže (vrátí počet přečtených/zapsaných sektorů menší než by podle požadavku správně měla), musíte disk považovat za vadný. I pokud následná volání vrací úspěch, nemusí být data na disk zapsaná správně / čtení může vracet nesmyslná data. Ideálně byste neměli disk po první chybě vůbec používat (ani pro čtení, ani pro zápis).

Co znamenají jednotlivé testy:

    Vytvoreni noveho pole - vytvoří se nové pole voláním create.
    Normalni operace (disky ok) - nastartuje RAID zařízení a zkusí provést sérii zápisů a čtení na tomto zařízení. Data se ukládají 2x (na Vaše RAID zařízení a do dalšího úložiště). Obsah obou úložišť je kontrolovaný. V tomto testu všechny disky pracují správně.
    Vypadek jednoho disku - testování je stejné jako v minulém bodě, ale jeden disk nepracuje správně.
    Obnova pole - degradované pole je obnoveno voláním resync. Simuluje obnovu RAID disku bez jeho vypnutí (on-line).
    Start pole (po rebootu) - operace simuluje odpojení RAID zařízení (stop) a jeho následné připojení (start). Na připojeném zařízení se následně provedou čtecí/zápisové operace. Pozor - po volání stop byste měli všechna data uložit na disky. Obsah paměti mezi touto dvojicí operací není zachován (garantované je pouze to, co jste si uložili na disk a následně při volání start načetli).
    Rozlozeni parity - kontroluje se, zda disky jsou rovnoměrně používané. Pokud je nějaký disk využíván výrazně více než ostatní, není zřejmě parita rovnoměrně rozmístěna mezi disky (RAID4 vs. RAID5).
    Vypadek jineho disku - simuluje výpadek jiného disku, pole je testováno v degradovaném režimu. Následně je pole v tomto degradovaném režimu zastaveno (stop).
    Obnova pole (off-line replace) - pole je nastartováno s tím, že vadný disk byl během odstávky vyměněn. Po nastartování pole (start) je spuštěna obnova (resync). Opět, mezi odstavením pole v minulém testu a nastartováním pole v tomto testu nemáte zachované žádné proměnné.
    Reakce na vypadek vice disku - simuluje výpadek dvou disků. RAID5 samozřejmě takový výpadek nedokáže překlenout a musí přejít do režimu FAILED.
