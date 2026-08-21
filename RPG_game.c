

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------
   CONSTANTS
   --------------------------------------------------------------------- */
#define MAX_NAME     50
#define MAX_LOGIN    40
#define QUIZ_POOL    20     /* how many random-pool questions exist   */
#define QUIZ_FIXED   3      /* always-asked questions                 */
#define QUIZ_RANDOM  4      /* random questions picked from the pool  */
#define SAVE_FILE    "story_save.txt"

/* ---------------------------------------------------------------------
   DATA TYPES
   --------------------------------------------------------------------- */

/* The hero. Works for both Free World Mode and Story Mode -- some
   fields (wife_name, pet_name, romance_name) are simply unused
   depending on which mode is active. */
typedef struct {
    char name[MAX_NAME];
    int  hp;                  /* current health                       */
    int  max_hp;               /* max health                           */
    int  strength_mult;        /* multiplies unarmed-move damage       */
    int  weapon_mult;          /* multiplies weapon damage             */

    int  has_knife;
    int  has_sword;
    int  gun_shots;
    int  grenades;
    int  healing_potions;
    int  machine_gun_uses;

    int  has_pet;              /* 0 none, 1 cat, 2 dog                 */
    char pet_name[MAX_NAME];

    char wife_name[MAX_NAME];       /* Story Mode only                 */
    int  has_companion;             /* Free World romance NPC          */
    char companion_name[MAX_NAME];
} Player;

/* A single enemy "template". Combat makes a working copy so the
   template's HP is never permanently changed. */
typedef struct {
    char name[MAX_NAME];
    int  hp;                 /* enemy HP                              */
    int  damage;              /* damage dealt to hero per hit          */
    int  miss_chance;         /* 0-100, chance the enemy's hit misses  */
    int  retaliate_every;     /* enemy strikes back after this many
                                  LANDED hits from the hero (1 = every
                                  hit, 2 = every second hit, etc.)     */
} Enemy;

typedef struct {
    char text[220];
    char option1[100];
    char option2[100];
    char option3[100];
    int  correct;             /* 1, 2 or 3                             */
} QuizQuestion;

/* =====================================================================
   SECTION 0 -- SMALL INPUT/OUTPUT HELPERS               [shared/CORE]
   ===================================================================== */

void pause_game(void)
{
    char line[16];
    printf("\n(press ENTER to continue) ");
    if (!fgets(line, sizeof(line), stdin)) exit(0);
}

/* Reads one integer in [min,max]. Re-prompts on anything invalid
   (letters, out-of-range numbers, empty lines, trailing garbage). */
int read_int(const char *prompt, int min, int max)
{
    char line[64];
    int value;
    char extra;

    while (1) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) exit(0);

        if (sscanf(line, " %d %c", &value, &extra) == 1 &&
            value >= min && value <= max) {
            return value;
        }
        printf("Please enter a number from %d to %d.\n", min, max);
    }
}

/* Reads a non-empty line of text into dest (spaces allowed). */
void read_line(const char *prompt, char *dest, int size)
{
    char buffer[256];
    while (1) {
        printf("%s", prompt);
        if (!fgets(buffer, sizeof(buffer), stdin)) exit(0);
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strlen(buffer) == 0) {
            printf("Please type something.\n");
            continue;
        }
        strncpy(dest, buffer, size - 1);
        dest[size - 1] = '\0';
        return;
    }
}

/* =====================================================================
   SECTION 1 -- QUIZ QUESTION BANK                         [QUIZ]
   Shared by the Free World S-Class Black Hulk fight and the Story
   Mode final boss fight, exactly as the design doc requires: one
   array, used from both places.
   ===================================================================== */

/* EDIT_QUIZ_QUESTIONS
   The 3 questions that are ALWAYS asked, in order. */
static QuizQuestion fixed_questions[QUIZ_FIXED] = {
    { "Who got to the moon for the very first time?",
      "Neil Armstrong", "Adnan Kondakar", "Rashed Hasan", 1 },
    { "What is the name of the highest mountain in the world?",
      "Aritro the Peak", "Mount Everest", "Tajingdong", 2 },
    { "What is the favourite word of K M M U sir?",
      "Ghorar Dim", "Hunky Punky", "Bondhu", 3 }
      /* ^ TODO: nobody but your instructor knows the "real" answer to
         this in-joke question -- change the correct option (1/2/3)
         to whatever it actually is. */
};

/* EDIT_QUIZ_QUESTIONS
   The pool that 4 random questions are drawn from each fight.
   (Every computed answer below was re-checked by hand -- the
   prototype code had 5 of these marked wrong.) */
static QuizQuestion question_pool[QUIZ_POOL] = {
    { "int x = 17 % 5 + 3 * 2; What is x?",              "8", "13", "10", 1 },
    { "int a=10,b=3; printf(\"%d\", a/b); Output?",       "3", "3.33", "4", 1 },
    { "int x=10; x = x % 4 + 2; What is x?",              "4", "6", "8", 1 },
    { "int x=5; if(x>2 && x<10) printf(\"YES\"); else printf(\"NO\"); Output?",
      "YES", "NO", "ERROR", 1 },
    { "Which function compares two strings in C?",        "strcmp()", "strcpy()", "strlen()", 1 },
    { "int x=2; for(i=0;i<4;i++) x=x+3; Final x?",        "11", "14", "15", 2 },
    { "int a=7,b=2; printf(\"%d\", a%b + a/b); Output?",  "3", "4", "5", 2 },
    { "Which keyword skips the current loop iteration?",  "break", "continue", "skip", 2 },
    { "int x=5; x++; x*=2; printf(\"%d\",x); Output?",    "10", "12", "11", 2 },
    { "int x = 2 + 3 * 4 % 5; What is x?",                "4", "6", "14", 1 },
    { "Who was first to reach the South Pole?",           "Robert Falcon Scott", "Roald Amundsen", "Ernest Shackleton", 2 },
    { "Largest ocean on Earth?",                          "Atlantic", "Indian", "Pacific", 3 },
    { "Country with the largest land area?",              "Canada", "Russia", "China", 2 },
    { "Who developed general relativity?",                "Newton", "Einstein", "Galileo", 2 },
    { "Smallest planet in the Solar System?",              "Mercury", "Mars", "Venus", 1 },
    { "Capital city of Australia?",                        "Sydney", "Melbourne", "Canberra", 3 },
    { "Element with chemical symbol \"Au\"?",               "Silver", "Gold", "Copper", 2 },
    { "Who wrote the novel \"1984\"?",                      "George Orwell", "Aldous Huxley", "Ernest Hemingway", 1 },
    { "Organ primarily responsible for filtering waste from the blood?",
      "Heart", "Kidney", "Liver", 2 },
    { "Civilization that built Machu Picchu?",              "Maya", "Aztec", "Inca", 3 }
};

/* =====================================================================
   SECTION 2 -- PLAYER HELPERS                              [CORE-01]
   ===================================================================== */

void heal_player(Player *p, int amount)
{
    p->hp += amount;
    if (p->hp > p->max_hp) p->hp = p->max_hp;
    printf("You recover %d hp. hp: %d/%d\n", amount, p->hp, p->max_hp);
}

void print_player(const Player *p)
{
    printf("\n------------------------------------------\n");
    printf("%-18s %d/%d hp\n", p->name, p->hp, p->max_hp);
    if (p->has_knife)        printf("Knife\n");
    if (p->has_sword)        printf("Sword\n");
    if (p->gun_shots > 0)    printf("Gun shots left: %d\n", p->gun_shots);
    if (p->grenades > 0)     printf("Grenades: %d\n", p->grenades);
    if (p->machine_gun_uses > 0) printf("Machine gun uses left: %d\n", p->machine_gun_uses);
    if (p->healing_potions > 0)  printf("Healing potions: %d\n", p->healing_potions);
    printf("------------------------------------------\n");
}

/* =====================================================================
   SECTION 3 -- COMBAT ENGINE                       [CORE-03, shared]
   One fight routine used everywhere: Free World random encounters,
   Story Mode room fights, and the Dwyen tutorial fight.
   The two Black Hulk quiz-fights use quiz_battle() instead (Section 4).
   ===================================================================== */

/* Presents the hero's move list for this turn and returns the damage
   dealt (0 if the action wasn't an attack, e.g. healing or viewing
   status), or -1 if the action was invalid/unavailable (turn is
   re-prompted, doesn't cost a turn). allow_unarmed selects between
   the Free World move list (punches/kicks + light weapons) and the
   Story Mode move list (sword/gun/grenade only, per [ST-04]). */
static int player_turn(Player *p, int allow_unarmed)
{
    int choice, damage;

    printf("\nChoose an action:\n");
    if (allow_unarmed) {
        printf(" 1) Punch in the belly   (5 dmg)\n");
        printf(" 2) Punch in the face    (10 dmg)\n");
        printf(" 3) Kick in the belly    (20 dmg)\n");
        printf(" 4) Knife                (12 dmg)%s\n", p->has_knife ? "" : "  [not carried]");
        printf(" 5) Sword                (50 dmg)%s\n", p->has_sword ? "" : "  [not carried]");
        printf(" 6) Gun                  (55 dmg, %d shots left)\n", p->gun_shots);
        printf(" 7) Grenade              (100 dmg, %d left)\n", p->grenades);
        printf(" 8) Machine gun          (200 dmg, %d uses left)\n", p->machine_gun_uses);
        printf(" 9) Healing potion       (+50 hp, %d left)\n", p->healing_potions);
        printf("10) View status\n");

        choice = read_int("> ", 1, 10);
        switch (choice) {
            case 1: return 5  * p->strength_mult;
            case 2: return 10 * p->strength_mult;
            case 3: return 20 * p->strength_mult;
            case 4:
                if (!p->has_knife) { printf("You don't have a knife.\n"); return -1; }
                return 12 * p->weapon_mult;
            case 5:
                if (!p->has_sword) { printf("You don't have a sword.\n"); return -1; }
                return 50 * p->weapon_mult;
            case 6:
                if (p->gun_shots <= 0) { printf("Out of gun shots.\n"); return -1; }
                p->gun_shots--;
                return 55 * p->weapon_mult;
            case 7:
                if (p->grenades <= 0) { printf("No grenades left.\n"); return -1; }
                p->grenades--;
                return 100 * p->weapon_mult;
            case 8:
                if (p->machine_gun_uses <= 0) { printf("No machine-gun uses left.\n"); return -1; }
                p->machine_gun_uses--;
                return 200 * p->weapon_mult;
            case 9:
                if (p->healing_potions <= 0) { printf("No healing potions left.\n"); return -1; }
                p->healing_potions--;
                heal_player(p, 50);
                return 0;
            default:
                print_player(p);
                return -1;
        }
    }

    /* Story Mode move list [ST-04] */
   /* Story Mode move list [ST-04] */
    printf(" 1) Sword           (%d dmg)%s\n", 70 * p->weapon_mult, p->has_sword ? "" : "  [not carried]");
    printf(" 2) Gun             (%d dmg, %d shots left)\n", 85 * p->weapon_mult, p->gun_shots);
    printf(" 3) Grenade         (%d dmg, %d left)\n", 200 * p->weapon_mult, p->grenades);
    printf(" 4) Healing potion  (+100 hp, %d left)\n", p->healing_potions);
    printf(" 5) View status\n");

    choice = read_int("> ", 1, 5);
    switch (choice) {
        case 1:
            if (!p->has_sword) { printf("You don't have a sword.\n"); return -1; }
            damage = 70 * p->weapon_mult;
            break;
        case 2:
            if (p->gun_shots <= 0) { printf("Out of gun shots.\n"); return -1; }
            p->gun_shots--;
            damage = 85 * p->weapon_mult;
            break;
        case 3:
            if (p->grenades <= 0) { printf("No grenades left.\n"); return -1; }
            p->grenades--;
            damage = 200 * p->weapon_mult;
            break;
        case 4:
            if (p->healing_potions <= 0) { printf("No healing potions left.\n"); return -1; }
            p->healing_potions--;
            heal_player(p, 100) ;
            return 0;
        default:
            print_player(p);
            return -1;
    }
    return damage;
}

/* The shared fight loop [CORE-03]. Returns 1 if the hero wins,
   0 if the hero dies. enemy_template is passed by value on purpose
   -- the copy inside this function is what takes damage, so calling
   code can reuse the same Enemy struct for future fights. */
int fight(Player *p, Enemy enemy_template, int allow_unarmed)
{
    Enemy enemy = enemy_template;
    int landed_hits = 0;   /* hero hits that actually damaged the enemy,
                               used for "hits back every Nth hit" enemies.
                               (Healing / invalid turns do NOT count --
                               this was a bug in the original prototype.) */
    int result;

    printf("\n============================================\n");
    printf("A %s appears!  (%d HP)\n", enemy.name, enemy.hp);
    printf("============================================\n");

   while (p->hp > 0 && enemy.hp > 0) {
        printf("\nYour hp: %d/%d   |   %s hp: %d\n", p->hp, p->max_hp, enemy.name, enemy.hp);

        /* --- OUT OF RESOURCES CHECK --- */
        int can_attack = p->has_sword  ||
                         p->has_knife ||
                         (allow_unarmed) ||
                         p->gun_shots > 0 ||
                         p->grenades > 0 ||
                         p->machine_gun_uses > 0 ||
                         p->healing_potions > 0;

        if (!can_attack) {
            printf("\n[OUT OF RESOURCES!] You have no ammo, grenades, or usable weapons left!\n");
            printf("Press Enter to brace for the enemy's attack...");

            while (getchar() != '\n');

            /* Enemy attacks automatically */
            p->hp -= enemy.damage;
            if (p->hp < 0) p->hp = 0;
            printf("\n%s hits you for %d damage! Your hp: %d/%d\n", enemy.name, enemy.damage, p->hp, p->max_hp);

            if (p->hp <= 0) break;
            continue; /* Skip turn and loop again */
        }

        result = player_turn(p, allow_unarmed);

        if (result == -1) continue;          /* invalid move, try again      */
        if (result == 0)  continue;          /* healed / viewed status */
        enemy.hp -= result;
        if (enemy.hp < 0) enemy.hp = 0;
        printf("You deal %d damage. %s hp: %d\n", result, enemy.name, enemy.hp);
        landed_hits++;

        if (enemy.hp <= 0) break;

        /* enemy retaliation */
        {
            int n = enemy.retaliate_every > 0 ? enemy.retaliate_every : 1;
            if (landed_hits % n == 0) {
                if (enemy.miss_chance > 0 && (rand() % 100) < enemy.miss_chance) {
                    printf("%s attacks but misses!\n", enemy.name);
                } else {
                    p->hp -= enemy.damage;
                    if (p->hp < 0) p->hp = 0;
                    printf("%s hits you for %d damage! Your hp: %d/%d\n",
                           enemy.name, enemy.damage, p->hp, p->max_hp);
                }
            }
        }
    }

    if (p->hp <= 0) {
        printf("\nYou were defeated by the %s.\n", enemy.name);
        return 0;
    }
    printf("\nYou defeated the %s!\n", enemy.name);
    return 1;
}

/* =====================================================================
   SECTION 4 -- QUIZ ENGINE                          [CORE-05, QUIZ]
   3 fixed + 4 random questions. Correct = hero hits the boss, wrong
   = hero gets hit. Reused for both the Free World S-Class Black Hulk
   and the Story Mode final boss.

   Damage-per-correct-answer is derived from the boss's own max HP so
   that a perfect run (7/7 correct) ALWAYS defeats the boss, no
   matter how much HP it has. (The prototype used a flat 125 damage,
   which made both Black Hulk fights mathematically unwinnable even
   with a perfect score -- that bug is fixed here.)
   ===================================================================== */

static void ask_one_question(QuizQuestion *q, Player *p, Enemy *boss, int per_correct_damage)
{
    printf("\n%s\n", q->text);
    printf(" 1) %s\n 2) %s\n 3) %s\n", q->option1, q->option2, q->option3);

    if (read_int("Answer: ", 1, 3) == q->correct) {
        boss->hp -= per_correct_damage;
        if (boss->hp < 0) boss->hp = 0;
        printf("Correct! You hit the %s for %d damage. Boss hp: %d\n",
               boss->name, per_correct_damage, boss->hp);
    } else {
        p->hp -= boss->damage;
        if (p->hp < 0) p->hp = 0;
        printf("Wrong! The %s hits you for %d damage. Your hp: %d/%d\n",
               boss->name, boss->damage, p->hp, p->max_hp);
    }
}

/* Picks QUIZ_RANDOM distinct indices out of question_pool. */
static void pick_random_questions(int chosen[QUIZ_RANDOM])
{
    int i, j, candidate, already_used;
    for (i = 0; i < QUIZ_RANDOM; i++) {
        do {
            candidate = rand() % QUIZ_POOL;
            already_used = 0;
            for (j = 0; j < i; j++)
                if (chosen[j] == candidate) already_used = 1;
        } while (already_used);
        chosen[i] = candidate;
    }
}

/* Returns 1 if the hero defeats the boss, 0 if the hero dies. */
int quiz_battle(Player *p, Enemy boss_template)
{
    Enemy boss = boss_template;
    int per_correct_damage = (boss.hp + (QUIZ_FIXED + QUIZ_RANDOM - 1)) / (QUIZ_FIXED + QUIZ_RANDOM);
    int chosen[QUIZ_RANDOM];
    int i;

    printf("\n============================================\n");
    printf("QUIZ BATTLE: %s  (%d HP)\n", boss.name, boss.hp);
    printf("%d fixed questions + %d random questions.\n", QUIZ_FIXED, QUIZ_RANDOM);
    printf("============================================\n");

    pick_random_questions(chosen);

    for (i = 0; i < QUIZ_FIXED && p->hp > 0 && boss.hp > 0; i++)
        ask_one_question(&fixed_questions[i], p, &boss, per_correct_damage);

    for (i = 0; i < QUIZ_RANDOM && p->hp > 0 && boss.hp > 0; i++)
        ask_one_question(&question_pool[chosen[i]], p, &boss, per_correct_damage);

    if (boss.hp <= 0) {
        printf("\nThe %s is defeated!\n", boss.name);
        return 1;
    }
    printf("\nThe %s defeated you.\n", boss.name);
    p->hp = 0;
    return 0;
}

/* =====================================================================
   SECTION 5 -- FREE WORLD MODE                                  [FW]
   No save/load. Restarts fully from the beginning on death.
   ===================================================================== */

/* EDIT_ENEMY_TABLE_FW */
static Enemy fw_enemy(int tier /* 1=B, 2=A, 3=S */, int index)
{
    static Enemy b_class[3] = {
        { "Goon",     100, 9,   0,  1 },
        { "Thief",     80, 6,   0,  1 },
        { "Wild Dog",  90, 19, 40,  1 },
    };
    static Enemy a_class[3] = {
        { "Robot",           250, 40,   0, 1 },
        { "Cyborg",          400, 60,  20, 1 },
        { "Mutant Dinosaur", 600, 120, 40, 1 },
    };
    static Enemy s_class[1] = {
        { "Black Hulk", 1000, 125, 20, 1 },
    };
    if (tier == 1) return b_class[index];
    if (tier == 2) return a_class[index];
    return s_class[index];
}

/* EDIT_ITEM_TABLE */
static void fw_item_pickup(Player *p)
{
    int choice;
    printf("\nYou spot a box of items on the ground. You can take only one... What will you take?\n");
    printf(" 1) Stick\n 2) Sword\n 3) Knife\n 4) Gun (20 shots)\n");
    printf(" 5) Torch\n 6) Healing potion\n 7) Grenade\n 8) Machine gun (2 uses)\n 9) Leave it\n");
    choice = read_int("> ", 1, 9);
    switch (choice) {
        case 1: printf("You pick up a stick. (just flavor, no effect)\n"); break;
        case 2: p->has_sword = 1; printf("Sword acquired!\n"); break;
        case 3: p->has_knife = 1; printf("Knife acquired!\n"); break;
        case 4: p->gun_shots += 20; printf("Gun acquired -- 20 shots loaded.\n"); break;
        case 5: printf("You pick up a torch. (just flavor, no effect)\n"); break;
        case 6: p->healing_potions++; printf("Healing potion acquired.\n"); break;
        case 7: p->grenades++; printf("Grenade acquired.\n"); break;
        case 8: p->machine_gun_uses += 2; printf("Machine gun acquired -- 2 uses loaded.\n"); break;
        default: printf("You leave it behind.\n"); break;
    }
}

static void fw_pet_event(Player *p)
{
    p->has_pet = 1 + rand() % 2;
    printf("\nA cute %s approaches and won't leave your side. It wants to come with you\n", p->has_pet == 1 ? "cat" : "dog");
    read_line("Give a name to this cute one: ", p->pet_name, MAX_NAME);   /* EDIT_NAMES */
    printf("%s is now traveling with you.\n", p->pet_name);
}

static void fw_romance_event(Player *p)
{
    printf("\nAmid the ruins, someone catches your eye -- and she is none other than your love, who is fighting to survive just like you..\n");
    read_line("Something inside you , all of a sudden , told you to confess your love towards her.. And you did it..And guess what , she blushed and said 'It took you that long, you morron'.. Now she is with you, what was her name again?? : ", p->companion_name, MAX_NAME);   /* EDIT_NAMES */
    p->has_companion = 1;
    printf("%s now roams the city with you. And you two make a wonderful couple\n", p->companion_name);
}

/* EDIT_LOVER_DIALOGUES
   Helper function to handle random dialogues after meeting his love */
static void fw_lover_dialogue(const Player *p)
{
    /* Array of 4 sample dialogue options */
    static const char *dialogues[4] = {
        "\"Are you always this slow, or are you just admiring the view?\" she teases with a smirk. You feel kinda teased",                  /* Teasing */
        "\"If we get eaten by a mutant, I'm never talking to you again!\" she mutters, shaking her head. You just keep on listening to her",             /* Funny */
        "She glances away, softly saying, \"...I'm actually really glad I found you out here, you know?\"",              /* Shy */
        "\"Hold still for a second,\" she says gently, brushing dust off your shoulder. \"Stay safe, okay?\""          /* Caring */
    };

    int index = rand() % 4; /* Pick a random dialogue sequence */
    printf("\n[ %s ]: %s\n", p->companion_name, dialogues[index]);
}

void run_free_world(void)
{
    Player p;
    int b_cleared = 0, a_cleared = 0;
    int distance_traveled = 0;
    int has_met_love = 0; /* Tracks romance so it happens ONLY ONCE */
    int current_zone = 0; /* 0 = Road, 1 = Desert */

    memset(&p, 0, sizeof(p));
    p.max_hp = 400;
    p.hp = 400;
    p.strength_mult = 1;
    p.weapon_mult = 1;

    printf("\n============================================\n");
    printf("FREE WORLD MODE\n Here you roam and see the ruins of the existing world\n");
    printf("============================================\n");
    read_line("Enter your hero's name: ", p.name, MAX_NAME);
    printf("Starting hp: 400\n");

    while (p.hp > 0) {
        /* Transition to Desert after traveling on the road for 8 steps */
        if (distance_traveled >= 8 && current_zone == 0) {
            current_zone = 1;
            printf("\n=======================================================\n");
            printf("The paved roads fade away into vast, scorching DESERT...\n");
            printf("=======================================================\n");
        }

        /* --- ZONE: ROAD --- */
        if (current_zone == 0) {
            printf("\nYou are traveling along the ruined road. (Distance: %d)\n", distance_traveled);
            printf("1) Roam the road\n");
            printf("2) Enter nearby building\n");
            printf("3) Quit to main menu\n");

            int choice = read_int("> ", 1, 3);
            if (choice == 3) {
                printf("You rest for now.\n");
                return;
            }

            if (choice == 1) {
                /* ROAMING THE ROAD */
                distance_traveled++;
                printf("\nYou roam further down the highway...\n");

                /* 1. Chance for One-Time Romance Event */
                if (!has_met_love && (rand() % 100 < 25 || distance_traveled >= 5)) {
                    fw_romance_event(&p);
                    has_met_love = 1;
                    continue;
                }

                /* 2. Random Dialogue with Lover (ONLY IF ALREADY MET) */
                if (has_met_love && rand() % 100 < 40) {
                    fw_lover_dialogue(&p);
                }

                /* 3. Item Pickup */
                if (rand() % 100 < 30) fw_item_pickup(&p);

                /* 4. Pet Event */
                if (!p.has_pet && rand() % 100 < 25) fw_pet_event(&p);

                /* 5. Enemy Encounter */
                if (rand() % 100 < 50) {
                    if (!b_cleared) {
                        int enemy_index = rand() % 3;
                        Enemy e = fw_enemy(1, enemy_index);
                        if (enemy_index == 0) {
                            int run_choice = read_int("\nA Goon blocks your way! 1) Fight  2) Run: ", 1, 2);
                            if (run_choice == 2 && rand() % 2 == 0) {
                                printf("You managed to escape safely!\n");
                                continue;
                            }
                        }
                        if (!fight(&p, e, 1)) break;
                    } else if (!a_cleared) {
                        Enemy e = fw_enemy(2, rand() % 3);
                        if (!fight(&p, e, 1)) break;
                    } else {
                        printf("\nAn S-Class enemy blocks the highway: the Black Hulk!\n");
                        quiz_battle(&p, fw_enemy(3, 0));
                        if (p.hp <= 0) break;
                    }
                }
            }
            else if (choice == 2) {
                /* ENTERING A BUILDING */
                int floors = 1 + rand() % 4;
                printf("\nYou enter an abandoned building with %d floor(s).\n", floors);

                for (int floor = 1; floor <= floors && p.hp > 0; floor++) {
                    printf("\n--- Building Floor %d ---\n", floor);

                    /* One-Time Romance can happen inside a building too */
                    if (!has_met_love && rand() % 100 < 20) {
                        fw_romance_event(&p);
                        has_met_love = 1;
                    }

                    /* Random Dialogue inside building (ONLY IF ALREADY MET) */
                    if (has_met_love && rand() % 100 < 35) {
                        fw_lover_dialogue(&p);
                    }

                    if (rand() % 100 < 35) fw_item_pickup(&p);

                    /* Building Enemy Encounter */
                    if (!b_cleared) {
                        Enemy e = fw_enemy(1, rand() % 3);
                        if (!fight(&p, e, 1)) break;
                    } else if (!a_cleared) {
                        Enemy e = fw_enemy(2, rand() % 3);
                        if (!fight(&p, e, 1)) break;
                    } else {
                        printf("\nThe Black Hulk lurks in this building!\n");
                        quiz_battle(&p, fw_enemy(3, 0));
                        if (p.hp <= 0) break;
                    }
                    if (p.hp > 0) pause_game();
                }
            }
        }
        /* --- ZONE: DESERT --- */
        else {
            printf("\nYou are in the vast, endless desert.\n");
            printf("1) Roam the desert wasteland\n");
            printf("2) Quit to main menu\n");

            int choice = read_int("> ", 1, 2);
            if (choice == 2) return;

            distance_traveled++;

            /* One-Time Romance Event */
            if (!has_met_love && rand() % 100 < 30) {
                fw_romance_event(&p);
                has_met_love = 1;
                continue;
            }

            /* Random Dialogue in Desert (ONLY IF ALREADY MET) */
            if (has_met_love && rand() % 100 < 40) {
                fw_lover_dialogue(&p);
            }

            /* Desert Events */
            if (rand() % 100 < 25) fw_item_pickup(&p);
            if (!p.has_pet && rand() % 100 < 20) fw_pet_event(&p);

            /* Desert Enemy Battles */
            if (rand() % 100 < 60) {
                if (!b_cleared) {
                    Enemy e = fw_enemy(1, rand() % 3);
                    if (!fight(&p, e, 1)) break;
                } else if (!a_cleared) {
                    Enemy e = fw_enemy(2, rand() % 3);
                    if (!fight(&p, e, 1)) break;
                } else {
                    printf("\nThe Black Hulk emerges from the desert dust!\n");
                    quiz_battle(&p, fw_enemy(3, 0));
                    if (p.hp <= 0) break;
                }
            }
        }

        if (p.hp > 0) pause_game();
    }

    printf("\n=========================================\n");
    printf("You have fallen, %s. Free World Mode restarts from the beginning.\n", p.name);
    printf("=========================================\n");
}
/* =====================================================================
   SECTION 6 -- STORY MODE                                        [ST]
   Has save/load and a single checkpoint (the Ground Floor of the
   Old Tower). Only a male hero, per the design doc.
   ===================================================================== */

/* EDIT_ENEMY_TABLE_ST */
static Enemy st_ground_enemy(int index)
{
    static Enemy table[4] = {
        { "Street Fighter", 80,  7,  0, 1 },
        { "Big Dog",         60, 10,  0, 1 },
        { "Swordsman",      120, 15,  0, 1 },
        { "Robot",          150, 40,  0, 1 },
    };
    return table[index];
}
static Enemy st_floor1_enemy(int index)
{
    static Enemy table[3] = {
        { "Robot",           150, 40, 0, 1 },
        { "Cyborg",           200, 45, 0, 1 },
        { "Mutant Dinosaur",  180, 50, 50, 1 },
    };
    return table[index];
}
static Enemy st_dragon(void)   { Enemy e = { "Mutant Dragon", 500, 100, 0, 1 }; return e; }
static Enemy st_mini_hulk(void){ Enemy e = { "Mini Hulk", 1000, 200, 0, 1 }; return e; }  /* hits back every 2nd landed hit */
static Enemy st_final_boss(void){ Enemy e = { "Black Hulk", 2000, 220, 0, 1 }; return e; }
static Enemy st_dwyen(void)    { Enemy e = { "Dwyen", 70, 5, 0, 1 }; return e; }

/* ---- persistence [CORE-02] --------------------------------------- */

typedef struct {
    int at_checkpoint;    /* 0 = not started, 1 = cleared ground floor */
    int floor1_cleared;
    int floor2_cleared;
    int comeback_done;
    int gun_pickup_available;    /* the hidden Floor-1 gun, still there?  */
    int potion_pickup_available; /* the Floor-2 potions, still there?     */
} StoryProgress;

/* Everything is written one field per line as plain text -- easy to
   read back with fgets(), and (unlike the prototype's fscanf("%s"...)
   approach) this safely supports names/passwords containing spaces. */
static int save_game(const char *user, const char *pass, const Player *p, const StoryProgress *prog)
{
    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) { printf("Could not write save file.\n"); return 0; }

    fprintf(fp, "%s\n%s\n", user, pass);
    fprintf(fp, "%s\n%s\n", p->name, p->wife_name);
    fprintf(fp, "%d %d %d %d\n", p->hp, p->max_hp, p->strength_mult, p->weapon_mult);
    fprintf(fp, "%d %d %d %d %d\n", p->has_knife, p->has_sword, p->gun_shots,
                                     p->grenades, p->healing_potions);
    fprintf(fp, "%d\n", p->machine_gun_uses);
    fprintf(fp, "%d %d %d %d %d %d\n", prog->at_checkpoint, prog->floor1_cleared,
             prog->floor2_cleared, prog->comeback_done,
             prog->gun_pickup_available, prog->potion_pickup_available);

    fclose(fp);
    return 1;
}

static void read_file_line(FILE *fp, char *dest, int size)
{
    if (!fgets(dest, size, fp)) dest[0] = '\0';
    dest[strcspn(dest, "\n")] = '\0';
}

/* returns 1 = loaded ok, 0 = no save file, -1 = wrong user/password */
static int load_game(const char *user, const char *pass, Player *p, StoryProgress *prog)
{
    FILE *fp = fopen(SAVE_FILE, "r");
    char stored_user[MAX_LOGIN], stored_pass[MAX_LOGIN];
    char line[64];

    if (!fp) return 0;

    read_file_line(fp, stored_user, MAX_LOGIN);
    read_file_line(fp, stored_pass, MAX_LOGIN);
    if (strcmp(user, stored_user) != 0 || strcmp(pass, stored_pass) != 0) {
        fclose(fp);
        return -1;
    }

    read_file_line(fp, p->name, MAX_NAME);
    read_file_line(fp, p->wife_name, MAX_NAME);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }
    sscanf(line, "%d %d %d %d", &p->hp, &p->max_hp, &p->strength_mult, &p->weapon_mult);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }
    sscanf(line, "%d %d %d %d %d", &p->has_knife, &p->has_sword, &p->gun_shots,
                                    &p->grenades, &p->healing_potions);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }
    sscanf(line, "%d", &p->machine_gun_uses);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }
    sscanf(line, "%d %d %d %d %d %d", &prog->at_checkpoint, &prog->floor1_cleared,
           &prog->floor2_cleared, &prog->comeback_done,
           &prog->gun_pickup_available, &prog->potion_pickup_available);

    fclose(fp);
    return 1;
}

/* ---- narrative beats ------------------------------------------------ */

/* EDIT_INTRO_DIALOGUE */
static void st_intro(void)
{
    printf("\n=========================================\n");
    printf("STORY MODE\n");
    printf("============================================\n");
    printf("\nYou are wealthy, successful, and a skilled martial artist.\However, you sure have a lot of enemies\n");
    printf("Tonight you are out to dinner with your lovely wife when a sudden ehplosion rocks the street outside.\n She crumbles from fear\n You want to go to check, but she stops you... You assure its ok and you will be back soon.\n");

    if (read_int("\n1) What happened next?  2) Skip to the game: ", 1, 2) == 1) {
        printf("\nYou go anyway... But that is the mistake you will forever remember\n");
        printf("When you turn back, your wife is gone.\n");
    } else {
        printf("\n[Skipped. Your wife has been kidnapped.]\n");
    }

    printf("\nThe kidnappers appear tied to enemies from your past business dealings.\n");
    printf("The police can't help. You decide to find their warehouse yourself.\n");

    if (read_int("\n1) What happened next?  2) Skip to the game: ", 1, 2) == 1) {
        printf("\nYou tell your old friend Detective Holmes and spend a day searching for leads -- nothing.\n");
        printf("Back home, a letter is waiting for you.\n");
    } else {
        printf("\n[Skipped. A letter from Holmes is waiting for you.]\n");
    }

    printf("\nHolmes' letter reads:\n");
    printf("  \"I have a lead. It's too dangerous to put in writing -- come see me\n");
    printf("   in person. And watch your back, they may be following you too.\"\n");
    pause_game();
}

/* EDIT_DWYEN_DIALOGUE */
static int st_dwyen_fight(Player *p)
{
    Enemy dwyen = st_dwyen();

    printf("\nOn the way to Holmes, a man in a dark coat steps out of an alley.\n");
    printf("\"Name's Dwyen. Mooz sends its regards.\"\n");
    printf("Dwyen attacks before you can react!\n");
    p->hp -= 25;
    if (p->hp < 0) p->hp = 0;
    printf("You take 25 damage. hp: %d/%d\n", p->hp, p->max_hp);

    if (p->hp <= 0) return 0;

    /* Dwyen is fought bare-handed only: punch (20) / kick (50), no
       weapons or items, per [ST-03]. We reuse the shared fight()
       engine but with a tiny custom move list, since Dwyen's moves
       don't match either of the two standard move lists. */
    while (p->hp > 0 && dwyen.hp > 0) {
        int choice, damage;
        printf("\nYour hp: %d | Dwyen hp: %d\n", p->hp, dwyen.hp);
        printf(" 1) Punch (20 dmg)\n 2) Kick (50 dmg)\n");
        choice = read_int("> ", 1, 2);
        damage = (choice == 1) ? 20 : 50;

        dwyen.hp -= damage;
        if (dwyen.hp < 0) dwyen.hp = 0;
        printf("You deal %d damage. Dwyen hp: %d\n", damage, dwyen.hp);

        if (dwyen.hp > 0) {
            p->hp -= dwyen.damage;
            if (p->hp < 0) p->hp = 0;
            printf("Dwyen hits back for %d. Your hp: %d/%d\n", dwyen.damage, p->hp, p->max_hp);
        }
    }

    if (p->hp <= 0) {
        printf("\nDwyen defeats you before you ever reach Holmes.\n");
        return 0;
    }
    printf("\nYou defeat Dwyen and continue on to Holmes.\n");
    return 1;
}

static void st_loadout(Player *p)
{
    int picked[4] = {0, 0, 0, 0};
    int count = 0, choice;

    printf("\nBefore entering the Old Tower, choose exactly 3 of these 4 [ST-04]:\n");
    while (count < 3) {
        printf("\n 1) Sword              (70 dmg, unlimited uses)%s\n", picked[0] ? "  [chosen]" : "");
        printf(" 2) Gun                (85 dmg, 14 shots)%s\n", picked[1] ? "  [chosen]" : "");
        printf(" 3) 5 grenades         (200 dmg each)%s\n", picked[2] ? "  [chosen]" : "");
        printf(" 4) 3 healing potions  (+50 hp each)%s\n", picked[3] ? "  [chosen]" : "");
        choice = read_int("Pick one: ", 1, 4);
        if (picked[choice - 1]) { printf("Already picked that one.\n"); continue; }
        picked[choice - 1] = 1;
        count++;
        if      (choice == 1) p->has_sword = 1;
        else if (choice == 2) p->gun_shots = 16;
        else if (choice == 3) p->grenades = 5;
        else                  p->healing_potions = 3;
    }
    printf("\nLoadout locked in.\n");
}

/* ---- The Old Tower [ST-05] ------------------------------------------ */

/* Ground floor: 3 rooms with enemies, then stairs. This is the
   CHECKPOINT -- reaching the stairs here is where a story-mode death
   sends the player back to. */
static int st_ground_floor(Player *p)
{
    int room, choice;
    printf("\n============================================\nGROUND FLOOR (checkpoint)\n============================================\n");

    for (room = 1; room <= 3; room++) {
        printf("\nRoom %d: an enemy blocks your way. And you have to beat it to move forward\n", room);
        if (!fight(p, st_ground_enemy(rand() % 4), 0)) return 0;

        if (room == 1) {
            printf("Exits: NORTH , EAST .\n");
            choice = read_int("1) East  2) North : ", 1, 2);
            if (choice == 2) { printf("\nYou reach the stairs up.\n"); return 1; }
        } else if (room == 2) {
            printf("Exit: NORTH .\n");
        } else {
            printf("Exit: WEST.\n");
        }
    }
    printf("\nYou reach the stairs up.\n");
    return 1;
}

/* 1st floor: Player chooses EAST (Room 2) or WEST (Room 3) from Room 1.
   Both paths lead to the Stairs. A gun hides in one of the rooms. */
static int st_first_floor(Player *p, int *gun_available)
{
    int choice, second_room, gun_room = 2 + rand() % 2;   /* room 2 or room 3 */

    printf("\n============================================\n1ST FLOOR\n============================================\n");

    /* --- ROOM 1 (Entry) --- */
    printf("\nRoom 1 (Entry): an enemy blocks the way. You need to wipe the floor with him to proceed\n");
    if (!fight(p, st_floor1_enemy(rand() % 3), 0)) return 0;

    printf("\nExits available: EAST  or WEST .\n");
    choice = read_int("Where do you want to go? 1) East  2) West: ", 1, 2);
    second_room = (choice == 1) ? 2 : 3;

    /* --- ROOM 2 / ROOM 3 --- */
    printf("\nRoom %d: an enemy blocks the way.You have your hands full today, man...\n", second_room);
    if (!fight(p, st_floor1_enemy(rand() % 3), 0)) return 0;

    /* Check for hidden gun */
    if (*gun_available && second_room == gun_room) {
        printf("\n!! You spot a gun hidden in the corner! You should not miss the chance to grab it\n");
        if (read_int("1) Take it  2) Leave it: ", 1, 2) == 1) {
            p->gun_shots += 20;
            *gun_available = 0;
            printf("Gun acquired -- 20 shots loaded.\n");
        }
    }

    /* Move to Stairs Room */
    printf("\nExit NORTH leads to the Stairs Room.\n");
    printf("1) Proceed to Stairs Room\n");
    read_int("> ", 1, 1);

    printf("\nYou reach the stairs up to proceed.\n");
    return 1;
}

/* 2nd floor:
   Room 1 (Entry) -> choice to go WEST (Room 2 / Potions) or NORTH (Room 3 / Dragon)
   Room 2 (Potions) -> exit NORTH leads to Room 3 (Dragon)
   Room 3 (Dragon) -> exit NORTH leads to Stairs Room (Mini Hulk) */
static int st_second_floor(Player *p, int *potion_available, int comeback_done)
{
    int choice;

    printf("\n============================================\n2ND FLOOR\n============================================\n");

    /* --- ROOM 1 (Entry) --- */
    printf("\nRoom 1 (Entry): empty. Exits available: WEST  or NORTH .\n");
    choice = read_int("Where do you want to go? 1) West   2) North : ", 1, 2);

    if (choice == 1) {
        /* --- ROOM 2 (Potion Room) --- */
        printf("\nRoom 2: empty, but you spot something on a shelf.\n");
        if (*potion_available) {
            if (read_int(" Well well...A pair of healing potions sits on a shelf. 1) Take  2) Leave: ", 1, 2) == 1) {
                p->healing_potions += 2;
                *potion_available = 0;
                printf("You take 2 healing potions.\n");
            }
        } else {
            printf("The shelf is empty.\n");
        }

        printf("\nExit NORTH leads to Room 3.\n");
        printf("1) Proceed to Room 3\n");
        read_int("> ", 1, 1);
    }

    /* --- ROOM 3 (Mutant Dragon) --- */
    printf("\nRoom 3: a Mutant Dragon roars and blocks the way! Damn, thats a big lizard\n");
    if (!fight(p, st_dragon(), 0)) return 0;

    printf("\nExit NORTH leads to the Stairs Room.\n");
    printf("1) Proceed to Stairs Room\n");
    read_int("> ", 1, 1);

    /* --- STAIRS ROOM (Mini Hulk) --- */
    printf("\nYou reach the Stairs Room -- a massive shape rises in the dark. The Mini Hulk! His name can have mini in it\n But surely he is not mini\n");

    if (!comeback_done) {
        /* Scripted loss on first attempt */
        printf("\nYou trade blows with the Mini Hulk, but it's simply too strong.\n And he beat the crap out of you\n");
        printf("It hurls you clean out of the Old Tower.\n");
        p->hp = 1;
        return 2;
    }

    /* Second attempt with comeback power-up */
    if (!fight(p, st_mini_hulk(), 0)) return 0;
    printf("\nThe Mini Hulk falls at last.\n");
    return 1;
}



/* EDIT_HOLMES_DIALOGUE */
static void st_comeback(Player *p)
{
    printf("\n============================================\nHOLMES FINDS YOU\n============================================\n");
    printf("\"I told you they wouldn't go down easy,.. man,.. they have beaten you up badly...\" Holmes says, hauling you up.\n");
    printf("\"Drink this. All of it.Its a potion i got from an old man named Chio, he said that he was your martial artist master\"\n");

    p->max_hp = 900;
    p->hp = 900;
    p->gun_shots +=10;
    p->grenades +=3;
    p->strength_mult *= 2;
    p->weapon_mult *= 2;
    p->healing_potions += 3;

    printf("\nThe super potion takes hold -- your strength and every weapon's power DOUBLE. And you got some extra weopons too\n");
    printf("Max hp is now 900, fully restored. You gain 2 extra healing potions.\n");
    printf("\nYou head back into the Old Tower.\n");
    pause_game();
}

/* EDIT_ENDING_DIALOGUE */
static void st_ending(const Player *p)
{
    printf("\n============================================\nFINAL ROOM\n============================================\n");
    printf("Through the last door, you find %s, shaken but alive.\n You grab her and ask if she is ok or not\n She says in shaken voice,'They kept me here for how long I don;t know..\n plsss take me away from here, I want to go home'\n Then you hug her tightly and assure her that its all ok, they wont be able to hurt you anymore\n", p->wife_name);
    printf("You cut her free and carry her out of the Old Tower, back to the life you built together.\n ");
    printf("\n*** CONGRATULATIONS -- YOU HAVE COMPLETED STORY MODE! ***\n");
}

void run_story_mode(void)
{
    Player p;
    StoryProgress prog = {0, 0, 0, 0, 1, 1};
    char user[MAX_LOGIN], pass[MAX_LOGIN];
    int choice, loaded, floor2_result;

    memset(&p, 0, sizeof(p));
    p.max_hp = 250;
    p.hp = 250;
    p.strength_mult = 1;
    p.weapon_mult = 1;

    printf("\n============================================\nSTORY MODE LOGIN\n============================================\n");
    printf(" 1) New player\n 2) Returning player\n");
    choice = read_int("> ", 1, 2);

    if (choice == 1) {
        read_line("Choose a user ID: ", user, MAX_LOGIN);
        read_line("Choose a password: ", pass, MAX_LOGIN);
        read_line("Your hero's name: ", p.name, MAX_NAME);          /* EDIT_NAMES */
        read_line("Your wife's name: ", p.wife_name, MAX_NAME);     /* EDIT_NAMES */

        st_intro();
        if (!st_dwyen_fight(&p)) {
            printf("\nGame over. Story Mode restarts from the beginning.\n");
            return;
        }

        printf("\nYou reach Detective Holmes.\n\"She's being held at the Old Tower,\" he says. \"Gear up.\"\n");
        st_loadout(&p);
        save_game(user, pass, &p, &prog);
    } else {
        read_line("User ID: ", user, MAX_LOGIN);
        read_line("Password: ", pass, MAX_LOGIN);
        loaded = load_game(user, pass, &p, &prog);
        if (loaded == 0)  { printf("\nNo save file found. Start a new game first.\n"); return; }
        if (loaded == -1) { printf("\nIncorrect user ID or password.\n"); return; }
        printf("\nWelcome back, %s.\n", p.name);
    }

    /* Ground floor (checkpoint) */
    while (!prog.at_checkpoint) {
        if (!st_ground_floor(&p)) {
            printf("\nYou died before reaching the checkpoint. Story restarts from the beginning.\n");
            remove(SAVE_FILE);
            return;
        }
        prog.at_checkpoint = 1;
        save_game(user, pass, &p, &prog);
    }

    /* 1st floor */
    while (!prog.floor1_cleared) {
        if (!st_first_floor(&p, &prog.gun_pickup_available)) {
            printf("\nYou died. Returning to the checkpoint (Ground Floor).\n");
            p.hp = p.max_hp;
            p.gun_shots += 5;
            p.grenades += 1;
            save_game(user, pass, &p, &prog);
            continue;
        }
        prog.floor1_cleared = 1;
        save_game(user, pass, &p, &prog);
    }

    /* Combined Floor 1 & Floor 2 loop */
    while (!prog.floor2_cleared) {


        /* 2nd floor */
        floor2_result = st_second_floor(&p, &prog.potion_pickup_available, prog.comeback_done);

        if (floor2_result == 0) {
            printf("\nYou died on Floor 2. Returning to Floor 1...\n");
            p.hp = p.max_hp;
            p.gun_shots +=10;
            p.grenades += 3;
            prog.floor1_cleared = 0;  /* Reset floor 1 so you must fight through it again */
            save_game(user, pass, &p, &prog);
            continue;
        }

        if (floor2_result == 2) {
            prog.comeback_done = 1;
            st_comeback(&p);          /* Gives super potion powers */
            prog.floor1_cleared = 0;  /* Sends you back to Floor 1 with super powers! */
            save_game(user, pass, &p, &prog);
            continue;
        }

        prog.floor2_cleared = 1;
        save_game(user, pass, &p, &prog);
    }

/* Final floor: boss room, then the ending. */
    printf("\n============================================\nFINAL FLOOR\n============================================\n");

    if (quiz_battle(&p, st_final_boss())) {
        st_ending(&p);
        remove(SAVE_FILE); /* story complete, clear the save */
        return;
    }

    /* If player loses the quiz battle against Black Hulk */
    printf("\nYou lost to the Black Hulk. Returning to the checkpoint (Ground Floor)...\n");

    p.hp = p.max_hp;             /* Restore health */
    prog.floor1_cleared = 0;     /* Reset story progress back to Ground Floor checkpoint */
    prog.floor2_cleared = 0;

    save_game(user, pass, &p, &prog);
    return;   }                  /* Return to main menu / checkpoint load */           /* Return to main menu / checkpoint load */  /* <--- ADD THIS CLOSING BRACE HERE TO CLOSE run_story_mode() */                    /* Return to main menu / checkpoint load */
/* =====================================================================
   SECTION 7 -- MAIN MENU                                     [CORE-04]
   ===================================================================== */

int main(void)
{
    int choice;
    srand((unsigned int) time(NULL));

    printf("=================================================\n");
    printf("           Echoes of the Old Tower\n");
    printf("=================================================\n");

    while (1) {
        printf("\nMAIN MENU\n 1) Free World Mode\n 2) Story Mode\n 3) Exit\n");
        choice = read_int("> ", 1, 3);

        if (choice == 1) run_free_world();
        else if (choice == 2) run_story_mode();
        else { printf("\nGoodbye!\n"); break; }
    }
    return 0;
}
