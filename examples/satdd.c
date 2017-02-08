#include <argp.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <sylvan_int.h>

/* Configuration */
static int workers = 0; // autodetect
static int verbose = 0;
static char* cnf_filename = NULL; // filename of CNF
static char* drat_filename = NULL; // filename of DRAT
static char* out_filename = NULL; // filename of output
static char* sizes = "27,29,26,28"; // default sizes
static char* variables = NULL; // variables to extract
static char* prop_units = NULL; // units to propagate first
static char* project_result = NULL; // units to propagate first

/* argp configuration */
static struct argp_option options[] =
{
    {"workers", 'w', "<workers>", 0, "Number of workers (default=0: autodetect)", 0},
    {"environment", 'e', "<variables>", 0, "Extract the environment of given variables", 0},
    {"units", 'u', "<units>", 0, "Units to propagate", 0},
    {"drat", 'd', "<proof file>", 0, "Check DRAT proof", 0},
    {"output", 'o', "<output file>", 0, "Write result to CNF", 0},
    {"project", 'p', "<project>", 0, "Project result", 0},
    {"table-sizes", 1, "<tablesize>,<tablemax>,<cachesize>,<cachemax>", 0, "Sizes of nodes table and operation cache as powers of 2", 0},
    {"verbose", 'v', 0, 0, "Set verbose", 0},
    {0, 0, 0, 0, 0, 0}
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
    switch (key) {
    case 'w':
        workers = atoi(arg);
        break;
    case 'v':
        verbose = 1;
        break;
    case 1:
        sizes = arg;
        break;
    case 'd':
        drat_filename = arg;
        break;
    case 'o':
        out_filename = arg;
        break;
    case 'e':
        variables = arg;
        break;
    case 'u':
        prop_units = arg;
        break;
    case 'p':
        project_result = arg;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num == 0) cnf_filename = arg;
        if (state->arg_num >= 2) argp_usage(state);
        break; 
    case ARGP_KEY_END:
        // if (state->arg_num < 1) argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, "<cnf_file>", 0, 0, 0, 0 };

/* Obtain current wallclock time */
static double
wctime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec + 1E-6 * tv.tv_usec);
}

static double t_start;
#define INFO(s, ...) fprintf(stdout, "\rc [% 8.2f] " s "\n", wctime()-t_start, ##__VA_ARGS__)
#define Abort(s, ...) { fprintf(stderr, "\rc [% 8.2f] " s "\n", wctime()-t_start, ##__VA_ARGS__); exit(-1); }

#define print_clause_db(db, nvars) CALL(fprint_clause_db, stdout, db, nvars)
#define fprint_clause_db(f, db, nvars) CALL(fprint_clause_db, f, db, nvars)
VOID_TASK_DECL_3(fprint_clause_db, FILE*, ZDD, int)

int literal_compare(const void *a, const void *b)
{
    int lita = *(int*)a;
    if (lita<0) lita=-lita;
    int litb = *(int*)b;
    if (litb<0) litb=-litb;
    return lita-litb;
}

int nvars, nclauses, nlits;

struct env
{
    int var;
    int n_clauses;
    int n_vars;
    int n_nodes;
    ZDD env;
    MTBDD sat;
};

struct env *environments;

int env_compare(const void *a, const void *b)
{
    struct env *ea = (struct env *)a;
    struct env *eb = (struct env *)b;
    return ea->n_nodes - eb->n_nodes;
}

TASK_1(ZDD, parse_cnf_file, FILE*, file)
{
    /* parse header */
    while (1) {
        int ch = fgetc(file);
        if (ch == EOF) {
            Abort("unexpected end-of-file");
        } else if (ch == 'c') {
            while ((ch = fgetc(file)) != '\n') {
                if (ch == EOF) Abort("unexpected end-of-file");
            }
            continue;
        } else if (ch != 'p') {
            Abort("unexpected characters");
        }
        if ((fscanf (file, " cnf %d %d", &nvars, &nclauses)) != 2) {
            Abort("invalid header");
        }
        if (nvars < 0 || nclauses < 0) {
            Abort("invalid header");
        }
        break;
    }
    
    /* setup arrays */
    int lits_size = 64, lits_count = 0;
    int *lits = (int*)malloc(sizeof(int[lits_size]));
    if (lits == NULL) Abort("out of memory");

    /* start with empty clause database */
    ZDD db = zdd_false;
    zdd_protect(&db);

    /* parse clauses */
    int read_clauses = 0;
    int last_literal = 0;
    nlits = 0;

    while (1) {
        int ch = fgetc(file);
        /* skip whitespace */
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue;
        /* skip comment lines */
        if (ch == 'c') {
            while ((ch = fgetc(file)) != '\n') {
                if (ch == EOF) Abort("unexpected end-of-file");
            }
            continue;
        }
        /* if EOF, check if good */
        if (ch == EOF) {
            if (last_literal != 0) Abort("zero missing");
            if (read_clauses < nclauses) Abort("clause(s) missing");
            break;
        }
        /* read next sign or digit */
        int sign;
        if (ch == '-') {
            sign = -1;
            ch = fgetc(file);
            if (!isdigit(ch)) Abort("expected digit after '-'");
        } else {
            sign = 1;
            if (!isdigit (ch)) Abort("expected digit or '-'");
        }
        int lit = ch - '0';
        /* parse rest of literal */
        while (isdigit(ch = fgetc(file))) lit = 10*lit + (ch - '0');
        if (lit > nvars) Abort("variable exceeds maximum");
        if (read_clauses >= nclauses) Abort("number of clauses more than expected");
        /* increase array size if needed */
        if (lits_count == lits_size) {
            lits_size += lits_size/2;
            lits = (int*)realloc(lits, sizeof(int[lits_size]));
            if (lits == NULL) Abort("out of memory");
        }
        if (lit != 0) {
            /* add literal to array */
            lits[lits_count++] = lit * sign;
            nlits++;
        } else {
            /* sort the literals by variable */
            qsort(lits, lits_count, sizeof(int), literal_compare);
            /* check for tautology or same variables */
            for (int i=0; i<lits_count-1; i++) {
                if (lits[i] == lits[i+1] || lits[i] == -lits[i+1]) Abort("variables twice in clause");
            }
            /* add clause to database */
            lits[lits_count] = 0;
            db = zdd_add_clause(db, lits);
            /* increase number of read clauses and reset literal count */
            read_clauses++;
            lits_count = 0;
        }
        /* set last literal and continue loop */
        last_literal = lit;
    }

    // printf("c total number of literals: %d\n", nlits);
    free(lits);
    zdd_unprotect(&db);

    return db;
}

int
test_empty_clause(ZDD set)
{
    if (set == zdd_true) return 1;
    if (set & zdd_complement) return 0;
    if (set == zdd_false) return 1;
    if (test_empty_clause(zdd_gethigh(set)) == 0) return 0;
    return test_empty_clause(zdd_getlow(set));
}

#define test_rup(db, units, clause, clause_len) CALL(test_rup, db, units, clause, clause_len)
TASK_4(int, test_rup, ZDD*, db, ZDD*, units, int*, clause, int, clause_len)
{
    /**
     * Add all literals in the clause NEGATED as units
     * (reverse order because more efficient)
     */
    ZDD clause_units = zdd_true;
    for (int i=0; i<clause_len; i++) {
        int unit = -clause[clause_len-i-1];
        uint32_t unitvar = unit < 0 ? (-2*unit) : (2*unit + 1);
        zdd_refs_push(clause_units);
        clause_units = zdd_set_add(clause_units, unitvar);
        zdd_refs_pop(1);
    }

    /**
     * Now propagate units until contradiction or done
     */
    ZDD db_check = *db;
    ZDD all_units = *units;
    ZDD new_units = clause_units;
    while (new_units != zdd_true) {
        zdd_refs_push(db_check);
        zdd_refs_push(new_units);
        zdd_refs_push(all_units);
        all_units = zdd_set_union(all_units, new_units);
        zdd_refs_pop(1);
        zdd_refs_push(all_units);
        if (zdd_clause_units_contradict(all_units)) {
            zdd_refs_pop(3);
            return 1; // found contradiction
        }
        db_check = zdd_clause_up(db_check, new_units);
        zdd_refs_push(db_check);
        new_units = zdd_clause_units(db_check);
        zdd_refs_pop(4);
        if (new_units == zdd_false) return 1; // found empty clause
    }

    /**
     * Not RUP, set *db and *units for further processing (RAT checking)
     * ... and return 0
     */
    *db = db_check;
    *units = all_units;
    return 0;
}

#define test_multirup(db, units, clauses, pivot) CALL(test_multirup, db, units, clauses, pivot)
TASK_4(int, test_multirup, ZDD, db, ZDD, units, ZDD, clauses, int, pivot)
{
    if (0) {
        int rat = 1;
        int32_t arr[nvars+1];
        ZDD res = zdd_clause_enum_first(clauses, arr);
        while (res != zdd_false) {
            int len = 0;
            for (int i=0; i<nvars; i++) {
                if (arr[i] == 0) break;
                else len++;
            }
            int32_t lits[len];
            // remove -pivot
            int j=0;
            for (int i=0; i<len; i++) {
                if (arr[i] != -pivot) lits[j++] = arr[i];
            }
            lits[j] = 0;
            len = j; // (should be len = len - 1)
            // sort
            qsort(lits, len, sizeof(int32_t), literal_compare);
            if (!test_rup(&db, &units, lits, len)) {
                rat = 0;
                break;
            }
            res = zdd_clause_enum_next(clauses, arr);
        }
        return rat;
    }

    if (clauses == zdd_true) return 1; // empty clause
    if (clauses == zdd_false) return 1; // empty set

    // in parallel test others...
    zddnode_t clauses_node = ZDD_GETNODE(clauses);
    SPAWN(test_multirup, db, units, zddnode_low(clauses, clauses_node), pivot);

    uint32_t var = zddnode_getvariable(clauses_node);
    ZDD high = zddnode_high(clauses, clauses_node);
    int lit = var & 1 ? var/2 : -1*(var/2);
    int res = 0;
    if (lit == pivot) {
        // skip pivot
        res = test_multirup(db, units, high, pivot);
    } else {
        ZDD new_units = zdd_set_add(zdd_true, var^1);
        while (new_units != zdd_true) {
            zdd_refs_push(db);
            zdd_refs_push(new_units);
            zdd_refs_push(units);
            units = zdd_set_union(units, new_units);
            zdd_refs_pop(1);
            zdd_refs_push(units);
            if (zdd_clause_units_contradict(units)) {
                zdd_refs_pop(3);
                res = 1;
                break;
            }
            db = zdd_clause_up(db, new_units);
            zdd_refs_push(db);
            new_units = zdd_clause_units(db);
            zdd_refs_pop(4);
            if (new_units == zdd_false) {
                res = 1;
                break;
            }
        }     
        if (res == 0) {
            res = test_multirup(db, units, high, pivot);
        }
    }
    
    int otherres = SYNC(test_multirup);
    return (res && otherres) ? 1 : 0;
}

TASK_3(int, parse_drat_file, FILE*, file, ZDD, db, ZDD, db_dom)
{
    // TODO: protect db
    zdd_protect(&db);

    /* setup arrays */
    int lits_size = 64, lits_count = 0;
    int *lits = (int*)malloc(sizeof(int[lits_size]));
    if (lits == NULL) Abort("out of memory");

    /* parse clauses */
    int last_literal = 0;
    int deleting = 0;

    while (1) {
        int ch = fgetc(file);
        /* skip whitespace */
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue;
        /* check deletion */
        if (ch == 'd') {
            deleting = 1;
            continue;
        }
        /* if EOF, check if good */
        if (ch == EOF) {
            if (last_literal != 0) Abort("zero missing");
            break;
        }
        /* read next sign or digit */
        int sign;
        if (ch == '-') {
            sign = -1;
            ch = fgetc(file);
            if (!isdigit(ch)) Abort("expected digit after '-'");
        } else {
            sign = 1;
            if (!isdigit (ch)) Abort("expected digit or '-'");
        }
        int lit = ch - '0';
        /* parse rest of literal */
        while (isdigit(ch = fgetc(file))) lit = 10*lit + (ch - '0');
        if (lit > nvars) Abort("variable exceeds maximum");
        /* increase array size if needed */
        if (lits_count == lits_size) {
            lits_size += lits_size/2;
            lits = (int*)realloc(lits, sizeof(int[lits_size]));
            if (lits == NULL) Abort("out of memory");
        }
        if (lit != 0) {
            /* add literal to array */
            lits[lits_count++] = lit * sign;
            nlits++;
        } else {
            int pivot = lits[0];
            /* sort the literals by variable */
            qsort(lits, lits_count, sizeof(int), literal_compare);
            /* check for tautology or same variables */
            for (int i=0; i<lits_count-1; i++) {
                if (lits[i] == lits[i+1] || lits[i] == -lits[i+1]) Abort("variables twice in clause");
            }
            /* end lits with 0 */
            lits[lits_count] = 0;
            /* if deleting, delete and continue */
            if (deleting) {
                // TODO zdd_remove_clause
                ZDD cl = zdd_clause(lits);
                db = zdd_ite(cl, zdd_false, db, db_dom);
                deleting = 0;
            } else {
                // TODO if verbose
                if (0) {
                    printf("checking lemma");
                    for (int i=0; i<lits_count; i++) {
                        printf(" %d", lits[i]);
                    }
                    printf("\n");
                }
                // check if RUP: add every literal (negated) as unit and propagate
                ZDD units = zdd_true;
                ZDD _db = db;
                if (test_rup(&_db, &units, lits, lits_count)) {
                    // INFO("Found RUP");
                } else {
                    // INFO("Found not RUP -- checking RAT with pivot %d", pivot);
                    /**
                     * Get all clauses with -pivot, then run RUP check on all (excluding -pivot)
                     */
                    ZDD neg_pivot = zdd_set_add(zdd_true, pivot < 0 ? -2*pivot+1 : 2*pivot);
                    ZDD db_env = zdd_clause_environment(db, neg_pivot);
                    // if (!test_empty_clause(db_env)) printf("oi\n");
                    if (!test_multirup(_db, units, db_env, pivot)) {
                        INFO("RAT check failed!");
                        return 0;
                    }
                }
                // add to database
                /* add clause to database */
                db = zdd_add_clause(db, lits);
            }
            /* reset literal count */
            lits_count = 0;
        }
        /* set last literal and continue loop */
        last_literal = lit;
    }

    free(lits);
    zdd_unprotect(&db);

    return 1;
}

/*
VOID_TASK_3(print_clause_cb, void*, context, int*, clause, int, len)
{
    // printf("c ");
    for (int i=0; i<len; i++) printf("%d ", clause[i]);
    printf("0\n");
    (void)context;
}
*/

VOID_TASK_IMPL_3(fprint_clause_db, FILE*, f, ZDD, db, int, nvars)
{
    // Get number of clauses
    int n_clauses = zdd_satcount(db);
    fprintf(f, "p cnf %d %d\n", nvars, n_clauses);

    int32_t arr[nvars+1];
    ZDD res = zdd_clause_enum_first(db, arr);
    while (res != zdd_false) {
        for (int i=0; i<nvars; i++) {
            if (arr[i] == 0) {
                fprintf(f, "0\n");
                break;
            } else {
                fprintf(f, "%d ", arr[i]);
            }
        }
        res = zdd_clause_enum_next(db, arr);
    }
}

char*
to_h(double size, char *buf) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    int i = 0;
    for (;size>1024;size/=1024) i++;
    sprintf(buf, "%.*f %s", i, size, units[i]);
    return buf;
}

void
print_set(ZDD set)
{
    if (set == zdd_true) return;
    printf("%d", zdd_getvar(set));
    if (zdd_gethigh(set) != zdd_true) printf(", ");
    print_set(zdd_gethigh(set));
}

void
print_units(ZDD set)
{
    if (set == zdd_true) return;
    uint32_t var = zdd_getvar(set);
    printf("%s%d", var&1?"":"-", var/2);
    if (zdd_gethigh(set) != zdd_true) printf(",");
    print_units(zdd_gethigh(set));
}

MTBDD
units_to_bdd(ZDD set)
{
    if (set == zdd_true) return mtbdd_true;
    MTBDD sub = units_to_bdd(zdd_gethigh(set));
    uint32_t var = zdd_getvar(set);
    if (var&1) {
        // positive
        return mtbdd_makenode(var/2, sylvan_false, sub);
    } else {
        // negative
        return mtbdd_makenode(var/2, sub, sylvan_false);
    }
}

ZDD
units_to_zdd(ZDD set)
{
    if (set == zdd_true) return zdd_false; // empty set
    ZDD sub = units_to_zdd(zdd_gethigh(set));
    uint32_t var = zdd_getvar(set);
    return zdd_makenode(var, sub, zdd_true);
}

VOID_TASK_0(gc_start)
{
    INFO("Starting garbage collection");
}

VOID_TASK_0(gc_end)
{
    INFO("Garbage collection done");
}

int
main(int argc, char **argv)
{
    t_start = wctime();

    argp_parse(&argp, argc, argv, 0, 0, 0);

    // Parse table sizes
    int tablesize, maxtablesize, cachesize, maxcachesize;
    if (sscanf(sizes, "%d,%d,%d,%d", &tablesize, &maxtablesize, &cachesize, &maxcachesize) != 4) {
        Abort("Invalid string for --table-sizes, try e.g. --table-sizes=23,28,22,27");
    }
    if (tablesize < 10 || maxtablesize < 10 || cachesize < 10 || maxcachesize < 10 ||
            tablesize > 40 || maxtablesize > 40 || cachesize > 40 || maxcachesize > 40) {
        Abort("Invalid string for --table-sizes, must be between 10 and 40");
    }
    if (tablesize > maxtablesize) {
        Abort("Invalid string for --table-sizes, tablesize is larger than maxtablesize");
    }
    if (cachesize > maxcachesize) {
        Abort("Invalid string for --table-sizes, cachesize is larger than maxcachesize");
    }

    // Report table sizes
    if (verbose) {
        char buf[32];
        to_h((1ULL<<maxtablesize)*24+(1ULL<<maxcachesize)*36, buf);
        INFO("Sylvan allocates %s virtual memory for nodes table and operation cache.\n", buf);
        to_h((1ULL<<tablesize)*24+(1ULL<<cachesize)*36, buf);
        INFO("Initial nodes table and operation cache requires %s.\n", buf);
    }

    // Init Lace
    lace_init(workers, 1000000); // auto-detect number of workers, use a 1,000,000 size task queue
    lace_startup(0, NULL, NULL); // auto-detect program stack, do not use a callback for startup

    LACE_ME;

    // Init Sylvan
    // sylvan_set_sizes(1LL<<tablesize, 1LL<<maxtablesize, 1LL<<cachesize, 1LL<<maxcachesize);
    // Give 12 GB memory
    sylvan_set_limits(12000000000, -1, 0);
    sylvan_init_package();
    sylvan_init_mtbdd();
    sylvan_init_zdd();

    // Set hooks for logging garbage collection
    // sylvan_gc_hook_pregc(TASK(gc_start));
    // sylvan_gc_hook_postgc(TASK(gc_end));
    
    ZDD db;
    zdd_protect(&db);

    // Open file
    if (cnf_filename != NULL) {
        FILE *f = fopen(cnf_filename, "r");
        if (f == NULL) Abort("Cannot open file %s!", cnf_filename);

        INFO("Opened %s.", cnf_filename);

        db = CALL(parse_cnf_file, f);

        // Close file
        fclose(f);
    } else {
        INFO("Reading from stdin.");
        db = CALL(parse_cnf_file, stdin);
    }

    INFO("Read %s, %d variables, %d clauses, %d literals.", cnf_filename, nvars, nclauses, nlits);

    // Compute the full domain of db (for ite)
    uint32_t dom_arr[nvars*2];
    for (int i=0; i<nvars*2; i++) dom_arr[i] = i+2;
    ZDD db_dom = zdd_set_from_array(dom_arr, nvars*2);
    zdd_protect(&db_dom);

    if (drat_filename != NULL) {
        FILE *f = fopen(drat_filename, "r");
        if (f == NULL) Abort("Cannot open file %s!", drat_filename);

        INFO("Opened %s.", drat_filename);

        int success = CALL(parse_drat_file, f, db, db_dom);

        // Close file
        fclose(f);
        
        if (success) {
            INFO("DRAT check good");
        } else {
            INFO("DRAT check bad");
        }
        exit(0);
    }

    //** HACK for testing subsumption **//
    /*
    ZDD test = zdd_ithvar(6); // 2
    db = zdd_clause_subsume(db, test);
    INFO("After self-subsumption: %.0lf clauses using %zu nodes.", zdd_satcount(db), zdd_nodecount(&db, 1));
    print_clause_db(db, nvars);
    exit(1);
    */
    // print_clause_db(db, nvars);

    ZDD original_db = db;
    ZDD all_units = zdd_true;
    zdd_protect(&original_db);
    zdd_protect(&all_units);

    sylvan_gc_disable();

    // Report that we have read the input file
    INFO("Stored %.0lf clauses using %zu ZDD nodes.", zdd_satcount(db), zdd_nodecount(&db, 1));

    db = zdd_clause_self_subsume(db);
    INFO("After self-subsumption: %.0lf clauses using %zu nodes.", zdd_satcount(db), zdd_nodecount(&db, 1));

    // Try the given unit propagation first
    if (prop_units != NULL) {
        char *str = strdup(prop_units);
        char *savePtr = str, *tok;
        ZDD units = zdd_true;
        while ((tok = strtok_r(savePtr, ",", &savePtr))) {
            int unit = atoi(tok);
            units = zdd_set_add(units, (unit < 0) ? (2*(-unit)) : (2*unit+1));
        }
        free(str);

        if (units != zdd_true) {
            all_units = zdd_set_union(all_units, units);
            INFO("Found %zu units!", zdd_set_count(units));
            if (zdd_clause_units_contradict(units)) {
                Abort("Units contradict! Aborting.\n");
            }
            db = zdd_clause_up(db, units);
        }
    }

    // Find all unit clauses
    ZDD units = zdd_clause_units(db);
    if (units == zdd_false) {
        Abort("The empty clause has been found!");
    }

    // Find all pure literals
    ZDD pure_literals = zdd_clause_pure(db_dom);
    units = zdd_or(units, pure_literals);

    if (units != zdd_true) {
        while (units != zdd_true) {
            all_units = zdd_set_union(all_units, units);
            // INFO("Found %zu new units!", zdd_set_count(units));
            if (zdd_clause_units_contradict(all_units)) {
                Abort("Units contradict! Aborting.");
            }
            db = zdd_clause_up(db, units);
            units = zdd_clause_units(db);
            if (units == zdd_false) {
                Abort("The empty clause has been found! Aborting.");
            }
            /**
             * EXPENSIVE pure literal check
             * A cheaper version would use a kill switch?
             */
            pure_literals = zdd_clause_pure(zdd_support(db));
            units = zdd_or(units, pure_literals);
        }

        // Compute support of environment
        ZDD vars_after = zdd_clause_support(db);
        size_t nvars_after = zdd_set_count(vars_after);
        INFO("After propagating units and pure literals: %.0lf clauses with %zu variables using %zu nodes.", zdd_satcount(db), nvars_after, zdd_nodecount(&db, 1));
    }

    if (variables != NULL) {
        char *str = strdup(variables);
        char *savePtr = str, *tok;
        ZDD lits = zdd_true;
        BDD vars = mtbdd_true;
        while ((tok = strtok_r(savePtr, ",", &savePtr))) {
            int var = atoi(tok);
            lits = zdd_set_add(lits, 2*var);
            lits = zdd_set_add(lits, 2*var+1);
            vars = mtbdd_set_add(vars, var);
        }
        free(str);

        // Extract the environment of *some* variable
        db = zdd_clause_environment(db, lits);

        // print_clause_db(db, nvars);

        // Compute support of environment
        ZDD vars_env = zdd_clause_support(db);
        int nvars_env = (int)zdd_set_count(vars_env);

        // Report
        INFO("Environment of \"%s\": %.0lf clauses, %d variables, %zu nodes.", variables, zdd_satcount(db), nvars_env, zdd_nodecount(&db, 1));

        // Compute BDD of satisfying assignments
        MTBDD sat = zdd_clause_sat(db, mtbdd_true);  // TODO: under partial assignment?
        INFO("Satisfying BDD: %zu nodes, %.0lf global assignments, %.0lf local assignments.", mtbdd_nodecount(sat), mtbdd_satcount(sat, nvars), mtbdd_satcount(sat, nvars_env));
        //FILE *fdot=fopen("sat.dot","w");mtbdd_fprintdot(fdot,sat);fclose(fdot);

        // Compute ISOC of BDD
        MTBDD xx;
        ZDD isoc = zdd_clause_isoc(sat, sat, &xx);

        // Compute Existential quatification of variables
        MTBDD quantified = sylvan_exists(sat, vars);
        MTBDD q_check;
        ZDD q_isoc = zdd_clause_isoc(quantified, quantified, &q_check);
        // INFO("BDD after Exists: %zu nodes, %.0lf global assignments, %.0lf local assignments.", mtbdd_nodecount(quantified), mtbdd_satcount(quantified, nvars), mtbdd_satcount(quantified, nvars_env));

        // print_clause_db(q_isoc, nvars);

        // Get number of clauses
        size_t env_n_clauses = zdd_satcount(db);
        size_t isoc_n_clauses = zdd_satcount(isoc);
        size_t q_isoc_n_clauses = zdd_satcount(q_isoc);

        INFO("#Clauses before=%zu, after=%zu, quant=%zu.", env_n_clauses, isoc_n_clauses, q_isoc_n_clauses);

        if (q_isoc_n_clauses <= env_n_clauses) {
            print_clause_db(q_isoc, nvars);
        } else if (isoc_n_clauses < env_n_clauses) {
            print_clause_db(isoc, nvars);
        }

        exit(1);
    }

    // Find out for each variable
    environments = (struct env*)calloc(sizeof(struct env), nvars);
    if (1) {
        INFO("Computing environments...");
        for (int i=1; i<=nvars; i++) {
            ZDD lits = zdd_set_from_array((uint32_t[]){2*i, 2*i+1}, 2);
            zdd_refs_push(lits);
            ZDD env = zdd_clause_environment(db, lits);
            zdd_refs_push(env);
            int n_env_clauses = zdd_satcount(env);
            ZDD envvars = zdd_clause_support(env);
            zdd_refs_pop(2);
            int n_env_vars = zdd_set_count(envvars);
            size_t n_env_nodes = zdd_nodecount(&env, 1);
            environments[i-1].var = i;
            environments[i-1].n_clauses = n_env_clauses;
            environments[i-1].n_vars = n_env_vars;
            environments[i-1].n_nodes = n_env_nodes;
        }

        // Sort environments
        INFO("Sorting environments...");
        qsort(environments, nvars, sizeof(struct env), env_compare);

        // List the environments (if verbose)
        if (verbose) {
            for (int i=0; i<nvars; i++) {
                struct env *e = environments+i;
                INFO("c Environment of variable %d has %d clauses, involves %d variables, and requires %d nodes.\n", e->var, e->n_clauses, e->n_vars, e->n_nodes);
            }
        }
    } else {
        for (int i=0; i<nvars; i++) {
            environments[i].var = i+1;
        }
    }

restart:
    for (int i=0; i<nvars; i++) {
        printf("%d/%d    \r", i, nvars);
        struct env *e = environments+i;

        /**
         * Report where we are
         */
        if (verbose) {
            INFO("Environment %d (of %d), variable %d", i+1, nvars, e->var);
        }

        /**
         * Recompute the environment
         */
        ZDD lits = zdd_set_from_array((uint32_t[]){2*e->var, 2*e->var+1}, 2);
        zdd_refs_push(lits);
        ZDD env = zdd_clause_environment(db, lits);

        /**
         * If environment empty, skip
         */
        if (env == zdd_false) {
            e->sat = mtbdd_true;
            e->env = zdd_false;
            continue; // skip empty
        }

        /**
         * If empty clause found, abort
         */
        if (env == zdd_false) {
            Abort("Empty clause found.");
        }

        /**
         * Compute the involved variables
         */
        ZDD env_vars = zdd_clause_support(env);
        MTBDD env_vars_bdd = zdd_set_to_mtbdd(env_vars);
        size_t env_vars_count = zdd_set_count(env_vars);

        /**
         * Report variable set of environment
         */
        /* 
        printf("Variables of %d: [", e->var);
        print_set(env_vars);
        printf("]\n");
        */

        /**
         * Compute second order environment
         */
        /*
        ZDD env_supp = zdd_support(env);
        ZDD env2 = zdd_clause_environment(db, env_supp);
        ZDD env2_vars = zdd_clause_support(env2);
        size_t env2_vars_count = zdd_set_count(env2_vars);
        INFO("Primary environment: %.0f clauses, %zu variables, %zu nodes.", zdd_satcount(env), env_vars_count, zdd_nodecount(&env, 1));
        INFO("Secondary environment: %.0f clauses, %zu variables, %zu nodes.", zdd_satcount(env2), env2_vars_count, zdd_nodecount(&env2, 1));
        */

        /**
         * Compute the satisfying assignments
         */
        MTBDD sat = zdd_clause_sat(env, mtbdd_true);

        /**
         * Compute projected state space based on earlier environments
         */
        MTBDD care = mtbdd_true;
        if (1) {
            for (int j=0; j<i; j++) {
                BDD q = sylvan_project(environments[j].sat, env_vars_bdd);
                care = sylvan_and(care, q);
            }
        }

        /**
         * Report if the dontcare set is non-empty
         */
        if (care != mtbdd_true && verbose) {
            // TODO: change into percentage
            printf("Preset for %d has %.0f models (%zu vars)\n", e->var, mtbdd_satcount(care, env_vars_count), env_vars_count);
        }

        /**
         * Compute lower and upper bound using projected state space
         */
        MTBDD lower = sylvan_and(sat, care);
        MTBDD upper = sat;

        // Compute ISOC of BDD
        MTBDD check;
        ZDD isoc = zdd_clause_isoc(lower, upper, &check);

        // Compute ISOC of BDD where we perform existential quantification
        MTBDD quantified = sylvan_exists(sat, sylvan_ithvar(e->var));
        MTBDD q_check;
        ZDD q_isoc = zdd_clause_isoc(quantified, quantified, &q_check);
        assert(q_check == quantified);

        /**
         * Check if the resulting set is between lower and upper bounds
         */
        if (lower != upper) {
            if (sylvan_and(lower, sylvan_not(check)) != sylvan_false) {
                printf("Resulting ISOC is below lower bound!\n");
                exit(1);
            }

            if (sylvan_and(check, sylvan_not(upper)) != sylvan_false) {
                printf("Resulting ISOC is above upper bound!\n");
                exit(1);
            }
        } else if (check != lower) {
            printf("Resulting ISOC does not match input BDD!\n");
            exit(1);
        }

        /**
         * Sanity check
         */
        if (check != zdd_clause_sat(isoc, mtbdd_true)) {
            printf("Resulting ISOC does not match output BDD!\n");
            exit(1);
        }

        // printf("c Result: %zu %zu\n", isoc, xx);
        // printf("c Check: %d\n", sat == xx ? 1 : 0);
        // printf("c Check 2: %d\n", zdd_clause_sat(isoc) == xx ? 1 : 0);
        //

        /**
         * Update original if number of clauses changed
         */
        int clauses_before = zdd_satcount(env);
        int clauses_after = zdd_satcount(isoc);
        int clauses_q = zdd_satcount(q_isoc);
        if (verbose) {
            printf("c Result %d %zu vars %d %d %d %s\n", e->var, zdd_set_count(env_vars), clauses_before, clauses_after, clauses_q, clauses_q<=clauses_before?"LEQ":"");
        }

        // TODO count number of nodes of result also

        if (clauses_after < clauses_before || clauses_q < clauses_before) {
            db = zdd_ite(env, zdd_false, db, db_dom);
            if (clauses_q < clauses_after) {
                db = zdd_or(db, q_isoc);
            } else {
                db = zdd_or(db, isoc);
            }

            /**
             * Report
             */
            //printf("c Environment of variable %d has %d clauses, involves %d variables, and requires %d nodes.\n", e->var, e->n_clauses, e->n_vars, e->n_nodes);
            //printf("c Satisfying BDD: %zu nodes, %.0lf global assignments, %.0lf local assignments.\n", mtbdd_nodecount(sat), mtbdd_satcount(sat, nvars), mtbdd_satcount(sat, env_n_vars));
            //printf("c From %d to %d clauses\n", e->n_clauses, nisoc_clauses);

            size_t n_clauses = zdd_satcount(db);
            INFO("Clauses after % 4d: %6zu (nodes: %6zu).", e->var, n_clauses, zdd_nodecount(&db, 1));

            ZDD units = zdd_clause_units(db);
            while (units != zdd_true) {
                if (units == zdd_false) {
                    Abort("The empty clause has been found! Aborting.");
                }
                all_units = zdd_set_union(all_units, units);
                printf("c Found %zu units!\n", zdd_set_count(units));
                if (zdd_clause_units_contradict(units)) {
                    Abort("Units contradict! Aborting.");
                }
                db = zdd_clause_up(db, units);
                units = zdd_clause_units(db);
            }
                
            //print_clause_db(e->env, nvars);
            //print_clause_db(isoc, nvars);
            goto restart;
        }

        /**
         * Update variables in environments array
         */
        e->sat = check;
        e->env = isoc;
    }

    // print units found
    if (0) {
        printf("c Units: ");
        if (all_units == zdd_true) printf("(none)");
        else print_units(all_units);
        printf("\n");
    }

    /**
     * Add units to final DB
     */
    ZDD units_clauses = units_to_zdd(all_units);
    db = zdd_or(db, units_clauses);

    if (out_filename) {
        FILE* f = fopen(out_filename, "w");
        fprint_clause_db(f, db, nvars);
        fclose(f);
    }

    exit(1);

    // Now the big question
    MTBDD bdd = units_to_bdd(all_units);
    // f=fopen("test.dot","w+");
    // mtbdd_fprintdot(f, zdd_clause_sat(db,mtbdd_true));
    // fclose(f);
    MTBDD res = zdd_clause_sat(db, bdd);
    // Sanity check
    // if (sylvan_and(bdd, res) != res) {
    //     printf("Sanity check failed!\n");
    //     MTBDD res = sylvan_and(bdd, zdd_clause_sat(db, mtbdd_true));
    // }
    // We can now quickly see if <res> contains stuff bound by the clauses
    int32_t arr[nvars+1];
    ZDD it = zdd_clause_enum_first(original_db, arr);
    while (it != zdd_false) {
        MTBDD clause = mtbdd_false;
        for (int i=0; i<nvars; i++) {
            if (arr[i] == 0) {
                break;
            } else {
                if (arr[i] < 0) {
                    clause = sylvan_or(clause, sylvan_nithvar(-arr[i]));
                } else {
                    clause = sylvan_or(clause, sylvan_ithvar(arr[i]));
                }
            }
        }
        if (sylvan_and(clause, res) != res) {
            printf("Error\n");
        }
        it = zdd_clause_enum_next(original_db, arr);
    }

    printf("Number of paths in the result: %.2f", sylvan_pathcount(res));
    printf("Number of assignments: %.0f\n", mtbdd_satcount(res, nvars));
    print_clause_db(db, nvars);

    // Given unit propagation first
    if (project_result != NULL) {
        char *str = strdup(project_result);
        char *savePtr = str, *tok;
        MTBDD vars = mtbdd_true;
        while ((tok = strtok_r(savePtr, ",", &savePtr))) {
            int var = atoi(tok);
            vars = mtbdd_set_add(vars, var);
        }
        free(str);

        res = sylvan_project(res, vars);
        printf("Number of assignments after projection: %.2f\n", sylvan_satcount(res, vars));
    }

    /**
     * Reduction algorithm
     * Given a list of variables, start with the first one.
     * Compute its environment and the BDD of its assignments.
     * Use ISOP to compute a new set of clauses.
     * Use the BDD to generate dontcare sets (via projection) for next ones, either partial or full.
     * (This could be iterated.)
     */

    // zdd_enum_seq(db, TASK(print_clause_cb), NULL);

    // Report Sylvan statistics (if SYLVAN_STATS is set)
    if (verbose) sylvan_stats_report(stdout);

    return 0;
}
