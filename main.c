#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define access _access
    #define F_OK 0
    #define R_OK 4
    #define popen _popen
    #define pclose _pclose
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <sys/wait.h>
#endif

#define MAX_LINE_LENGTH 1024
#define MAX_PATH_LENGTH 512
#define MAX_COMMITS 1000
#define MAX_FILES 500
#define MAX_BRANCHES 100

typedef struct {
    char hash[41];
    char author[256];
    char date[64];
    char message[512];
    int files_changed;
    int insertions;
    int deletions;
} commit_info_t;

typedef struct {
    char name[256];
    char last_commit[41];
    int is_merged;
    int commits_ahead;
} branch_info_t;

typedef struct {
    char path[MAX_PATH_LENGTH];
    int changes;
    char last_commit[41];
    char last_author[256];
} file_info_t;

commit_info_t commits[MAX_COMMITS];
branch_info_t branches[MAX_BRANCHES];
file_info_t files[MAX_FILES];
int commit_count = 0;
int branch_count = 0;
int file_count = 0;

// ==================== GIT COMMAND EXECUTION ====================

int run_git_command(const char *format, ...) {
    char command[MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(command, sizeof(command), format, args);
    va_end(args);

    #ifdef _WIN32
    char full_command[MAX_LINE_LENGTH + 10];
    snprintf(full_command, sizeof(full_command), "%s > NUL 2>&1", command);
    #else
    char full_command[MAX_LINE_LENGTH + 15];
    snprintf(full_command, sizeof(full_command), "%s > /dev/null 2>&1", command);
    #endif

    FILE *fp = popen(full_command, "r");
    if (!fp) {
        return -1;
    }

    int result = pclose(fp);
    
#ifdef _WIN32
    return result;
#else
    return WEXITSTATUS(result);
#endif
}

char* run_git_command_output(const char *format, ...) {
    static char output[8192];
    char command[MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(command, sizeof(command), format, args);
    va_end(args);

    FILE *fp = popen(command, "r");
    if (!fp) {
        return NULL;
    }

    output[0] = '\0';
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), fp)) {
        strncat(output, buffer, sizeof(output) - strlen(output) - 1);
    }

    pclose(fp);
    
    size_t len = strlen(output);
    if (len > 0 && output[len-1] == '\n') {
        output[len-1] = '\0';
    }
    
    return output;
}

int is_git_repository() {
    #ifdef _WIN32
    return run_git_command("git rev-parse --git-dir") == 0;
    #else
    return run_git_command("git rev-parse --git-dir") == 0;
    #endif
}

// ==================== COMMIT ANALYSIS ====================

void load_commit_history() {
    char *output = run_git_command_output("git log --oneline --format=\"%%H %%an %%ad %%s\" --date=short -1000");
    if (!output) {
        printf("Failed to get commit history\n");
        return;
    }

    char *line = strtok(output, "\n");
    while (line && commit_count < MAX_COMMITS) {
        commit_info_t *commit = &commits[commit_count];
        
        char *hash = strtok(line, " ");
        char *author = strtok(NULL, " ");
        char *date = strtok(NULL, " ");
        char *message = strtok(NULL, "\n");
        
        if (hash && author && date && message) {
            strncpy(commit->hash, hash, 40);
            commit->hash[40] = '\0';
            
            strncpy(commit->author, author, 255);
            commit->author[255] = '\0';
            
            strncpy(commit->date, date, 63);
            commit->date[63] = '\0';
            
            strncpy(commit->message, message, 511);
            commit->message[511] = '\0';
            
            char stats_cmd[512];
            snprintf(stats_cmd, sizeof(stats_cmd), "git show --stat %s", hash);
            char *stats_output = run_git_command_output(stats_cmd);
            
            if (stats_output) {
                commit->files_changed = 0;
                commit->insertions = 0;
                commit->deletions = 0;
                
                char *stat_line = strtok(stats_output, "\n");
                while (stat_line) {
                    if (strstr(stat_line, "file") != NULL) {
                        char *files_ptr = strstr(stat_line, "file");
                        if (files_ptr) {
                            char *num_start = files_ptr;
                            while (num_start > stat_line && isdigit(*(num_start-1))) {
                                num_start--;
                            }
                            commit->files_changed = atoi(num_start);
                        }
                        
                        char *insertions_ptr = strstr(stat_line, "insertion");
                        if (insertions_ptr) {
                            char *num_start = insertions_ptr;
                            while (num_start > stat_line && (isdigit(*(num_start-1)) || *(num_start-1) == '+' || *(num_start-1) == '-')) {
                                num_start--;
                            }
                            commit->insertions = atoi(num_start);
                        }
                        
                        char *deletions_ptr = strstr(stat_line, "deletion");
                        if (deletions_ptr) {
                            char *num_start = deletions_ptr;
                            while (num_start > stat_line && (isdigit(*(num_start-1)) || *(num_start-1) == '+' || *(num_start-1) == '-')) {
                                num_start--;
                            }
                            commit->deletions = atoi(num_start);
                        }
                        break;
                    }
                    stat_line = strtok(NULL, "\n");
                }
            }
            
            commit_count++;
        }
        line = strtok(NULL, "\n");
    }
}

void show_commit_summary() {
    printf("📊 Repository Analysis\n");
    printf("=====================\n");
    printf("Total commits: %d\n", commit_count);
    
    if (commit_count == 0) {
        printf("No commit history found. Make sure you have commits in your repository.\n\n");
        return;
    }
    
    int total_insertions = 0, total_deletions = 0;
    for (int i = 0; i < commit_count; i++) {
        total_insertions += commits[i].insertions;
        total_deletions += commits[i].deletions;
    }
    printf("Total changes: +%d -%d lines\n", total_insertions, total_deletions);
    
    char authors[50][256];
    int author_counts[50] = {0};
    int author_count = 0;
    
    for (int i = 0; i < commit_count; i++) {
        int found = 0;
        for (int j = 0; j < author_count; j++) {
            if (strcmp(authors[j], commits[i].author) == 0) {
                author_counts[j]++;
                found = 1;
                break;
            }
        }
        if (!found && author_count < 50) {
            strcpy(authors[author_count], commits[i].author);
            author_counts[author_count] = 1;
            author_count++;
        }
    }
    
    int max_commits = 0;
    char top_author[256] = "";
    for (int i = 0; i < author_count; i++) {
        if (author_counts[i] > max_commits) {
            max_commits = author_counts[i];
            strcpy(top_author, authors[i]);
        }
    }
    
    printf("Most active author: %s (%d commits)\n", top_author, max_commits);
    
    if (commit_count > 0) {
        printf("Latest commit: %s by %s\n", commits[0].date, commits[0].author);
    }
    printf("\n");
}

// ==================== BRANCH ANALYSIS ====================

void load_branch_info() {
    char *output = run_git_command_output("git branch -v");
    if (!output) {
        printf("Failed to get branch information\n");
        return;
    }

    char *line = strtok(output, "\n");
    while (line && branch_count < MAX_BRANCHES) {
        branch_info_t *branch = &branches[branch_count];
        
        const char *ptr = line;
        while (*ptr == ' ' || *ptr == '*') ptr++;
        
        const char *name_end = ptr;
        while (*name_end && *name_end != ' ') name_end++;
        
        if (name_end > ptr) {
            size_t name_len = name_end - ptr;
            strncpy(branch->name, ptr, name_len);
            branch->name[name_len] = '\0';
            
            const char *commit_ptr = name_end;
            while (*commit_ptr && *commit_ptr == ' ') commit_ptr++;
            
            if (strlen(commit_ptr) >= 7) {
                strncpy(branch->last_commit, commit_ptr, 7);
                branch->last_commit[7] = '\0';
            }
            
            branch->is_merged = 0;
            if (strcmp(branch->name, "main") != 0 && strcmp(branch->name, "master") != 0) {
                #ifdef _WIN32
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "git branch --merged main 2>NUL | findstr \"^  %s$\"", branch->name);
                if (run_git_command(cmd) == 0) {
                    branch->is_merged = 1;
                } else {
                    snprintf(cmd, sizeof(cmd), "git branch --merged master 2>NUL | findstr \"^  %s$\"", branch->name);
                    if (run_git_command(cmd) == 0) {
                        branch->is_merged = 1;
                    }
                }
                #else
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "git branch --merged main 2>/dev/null | grep -q \"^  %s$\"", branch->name);
                if (run_git_command(cmd) == 0) {
                    branch->is_merged = 1;
                } else {
                    snprintf(cmd, sizeof(cmd), "git branch --merged master 2>/dev/null | grep -q \"^  %s$\"", branch->name);
                    if (run_git_command(cmd) == 0) {
                        branch->is_merged = 1;
                    }
                }
                #endif
            }
            
            branch_count++;
        }
        line = strtok(NULL, "\n");
    }
}

void show_branch_analysis() {
    printf("🌿 Branch Analysis\n");
    printf("=================\n");
    
    if (branch_count == 0) {
        printf("No branches found.\n\n");
        return;
    }
    
    int merged_branches = 0;
    int active_branches = 0;
    char current_branch[256] = "";
    
    char *current = run_git_command_output("git branch --show-current");
    if (current) {
        strncpy(current_branch, current, 255);
        current_branch[255] = '\0';
    }
    
    for (int i = 0; i < branch_count; i++) {
        if (branches[i].is_merged) {
            merged_branches++;
        } else if (strcmp(branches[i].name, "main") != 0 && 
                   strcmp(branches[i].name, "master") != 0 &&
                   strcmp(branches[i].name, current_branch) != 0) {
            active_branches++;
        }
    }
    
    printf("Total branches: %d\n", branch_count);
    printf("Current branch: %s\n", current_branch[0] ? current_branch : "unknown");
    printf("Active branches: %d\n", active_branches);
    printf("Merged branches (can be deleted): %d\n", merged_branches);
    
    if (merged_branches > 0) {
        printf("\n🚮 Branches that can be safely deleted:\n");
        for (int i = 0; i < branch_count; i++) {
            if (branches[i].is_merged) {
                printf("  • %s\n", branches[i].name);
            }
        }
    }
    printf("\n");
}

// ==================== FILE ANALYSIS ====================

void load_file_analysis() {
    char *output = run_git_command_output("git ls-files");
    if (!output) {
        printf("Failed to get file list\n");
        return;
    }

    char *line = strtok(output, "\n");
    while (line && file_count < MAX_FILES) {
        file_info_t *file = &files[file_count];
        strncpy(file->path, line, MAX_PATH_LENGTH - 1);
        file->path[MAX_PATH_LENGTH - 1] = '\0';
        
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "git log --oneline -- \"%s\" | find /c /v \"\"", line);
        #ifndef _WIN32
        snprintf(cmd, sizeof(cmd), "git log --oneline -- \"%s\" | wc -l", line);
        #endif
        
        char *changes_output = run_git_command_output(cmd);
        if (changes_output) {
            file->changes = atoi(changes_output);
        } else {
            file->changes = 1;
        }
        
        snprintf(cmd, sizeof(cmd), "git log -1 --format=%%an -- \"%s\"", line);
        char *author = run_git_command_output(cmd);
        if (author) {
            strncpy(file->last_author, author, 255);
            file->last_author[255] = '\0';
            char *newline = strchr(file->last_author, '\n');
            if (newline) *newline = '\0';
        } else {
            strcpy(file->last_author, "Unknown");
        }
        
        file_count++;
        line = strtok(NULL, "\n");
    }
}

void show_hot_files() {
    printf("🔥 Frequently Changed Files\n");
    printf("===========================\n");
    
    if (file_count == 0) {
        printf("No files found in repository.\n\n");
        return;
    }
    
    for (int i = 0; i < file_count - 1; i++) {
        for (int j = 0; j < file_count - i - 1; j++) {
            if (files[j].changes < files[j + 1].changes) {
                file_info_t temp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = temp;
            }
        }
    }
    
    int count = (file_count < 10) ? file_count : 10;
    printf("Top %d most frequently changed files:\n", count);
    for (int i = 0; i < count; i++) {
        printf("%3d changes: %s (last by: %s)\n", files[i].changes, files[i].path, files[i].last_author);
    }
    printf("\n");
}

// ==================== SMART BLAME ====================

void smart_blame(const char *filepath) {
    if (access(filepath, R_OK) != 0) {
        printf("❌ File not found or not readable: %s\n", filepath);
        return;
    }
    
    printf("🔍 Smart Blame: %s\n", filepath);
    printf("==========================================\n");
    
    char *output = run_git_command_output("git blame -s \"%s\"", filepath);
    if (!output) {
        printf("Failed to get blame information\n");
        return;
    }
    
    char *line = strtok(output, "\n");
    int line_num = 1;
    int lines_shown = 0;
    
    while (line && lines_shown < 10) {
        char *hash = line;
        
        char *author_start = strchr(line, '(');
        if (author_start) {
            author_start++;
            char *author_end = strchr(author_start, ' ');
            if (author_end) {
                char author[256] = {0};
                strncpy(author, author_start, author_end - author_start);
                author[author_end - author_start] = '\0';
                
                char msg_cmd[256];
                snprintf(msg_cmd, sizeof(msg_cmd), "git show -s --format=%%s %s", hash);
                char *message = run_git_command_output(msg_cmd);
                
                if (message) {
                    message[strcspn(message, "\n")] = 0;
                    printf("%3d: %s - %s\n", line_num, author, message);
                    lines_shown++;
                }
            }
        }
        
        line_num++;
        line = strtok(NULL, "\n");
    }
    
    if (lines_shown == 10) {
        printf("... (showing first 10 lines)\n");
    }
    printf("\n");
}

// ==================== CLEANUP SUGGESTIONS ====================

void show_cleanup_suggestions() {
    printf("🧹 Cleanup Suggestions\n");
    printf("=====================\n");
    
    char *status = run_git_command_output("git status --porcelain");
    if (status && strlen(status) > 0) {
        int untracked = 0, modified = 0;
        char *line = strtok(status, "\n");
        while (line) {
            if (line[0] == '?') untracked++;
            else modified++;
            line = strtok(NULL, "\n");
        }
        
        if (modified > 0) printf("📝 Modified files: %d (consider committing changes)\n", modified);
        if (untracked > 0) printf("❓ Untracked files: %d (consider adding to .gitignore)\n", untracked);
        
        if (modified == 0 && untracked == 0) {
            printf("✅ Working directory is clean\n");
        }
    } else {
        printf("✅ Working directory is clean\n");
    }
    
    char *stash = run_git_command_output("git stash list");
    if (stash && strlen(stash) > 0) {
        int stash_count = 0;
        char *line = strtok(stash, "\n");
        while (line) {
            stash_count++;
            line = strtok(NULL, "\n");
        }
        printf("💼 Stashed changes: %d (consider reviewing or applying)\n", stash_count);
    }
    
    printf("\n");
}

// ==================== AI COMMIT SUGGESTIONS ====================

void analyze_changes_for_commit_type(char* diff_output, char* type, char* description) {
    if (strstr(diff_output, "+++ b/") && strstr(diff_output, "--- /dev/null")) {
        strcpy(type, "feat");
        strcpy(description, "add new feature");
    } else if (strstr(diff_output, "fix") || strstr(diff_output, "bug") || strstr(diff_output, "error")) {
        strcpy(type, "fix");
        strcpy(description, "resolve issue");
    } else if (strstr(diff_output, "refactor") || strstr(diff_output, "cleanup") || strstr(diff_output, "optimize")) {
        strcpy(type, "refactor");
        strcpy(description, "improve code structure");
    } else if (strstr(diff_output, "test")) {
        strcpy(type, "test");
        strcpy(description, "add or update tests");
    } else if (strstr(diff_output, "doc") || strstr(diff_output, "readme") || strstr(diff_output, "comment")) {
        strcpy(type, "docs");
        strcpy(description, "update documentation");
    } else {
        strcpy(type, "chore");
        strcpy(description, "maintenance tasks");
    }
}

char* extract_commit_subject(char* diff_output) {
    static char subject[200];
    strcpy(subject, "implement changes");
    
    char* line = strtok(diff_output, "\n");
    while (line) {
        if (strstr(line, "+class ") || strstr(line, "+function ") || strstr(line, "+def ") || strstr(line, "+fn ")) {
            char* name_start = strstr(line, "class ");
            if (!name_start) name_start = strstr(line, "function ");
            if (!name_start) name_start = strstr(line, "def ");
            if (!name_start) name_start = strstr(line, "fn ");
            
            if (name_start) {
                name_start += 6;
                char* name_end = strchr(name_start, ' ');
                if (name_end && (name_end - name_start) < 50) {
                    strncpy(subject, name_start, name_end - name_start);
                    subject[name_end - name_start] = '\0';
                    strcat(subject, " implementation");
                    break;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    
    return subject;
}

int extract_key_changes(char* diff_output, char* output) {
    strcpy(output, "code changes");
    
    if (strstr(diff_output, "TODO") || strstr(diff_output, "FIXME")) {
        strcpy(output, "address code comments");
        return 1;
    }
    if (strstr(diff_output, "import") || strstr(diff_output, "include") || strstr(diff_output, "require")) {
        strcpy(output, "update dependencies");
        return 1;
    }
    if (strstr(diff_output, "config") || strstr(diff_output, "setting")) {
        strcpy(output, "update configuration");
        return 1;
    }
    
    return 0;
}

void generate_commit_suggestions() {
    printf("🤖 AI Commit Message Suggestions\n");
    printf("===============================\n");
    
    char* diff_output = run_git_command_output("git diff --staged");
    if (!diff_output || strlen(diff_output) == 0) {
        printf("No staged changes found. Use 'git add' to stage changes first.\n\n");
        return;
    }
    
    char commit_type[20];
    char description[100];
    analyze_changes_for_commit_type(diff_output, commit_type, description);
    
    int files_changed = 0;
    char* line = strtok(diff_output, "\n");
    while (line) {
        if (strstr(line, "diff --git") != NULL) {
            files_changed++;
        }
        line = strtok(NULL, "\n");
    }
    
    printf("Based on your changes (%d files, %s):\n\n", files_changed, description);
    
    printf("1. %s: %s\n", commit_type, extract_commit_subject(diff_output));
    printf("2. %s: update %d files for %s\n", commit_type, files_changed, description);
    
    char specific_desc[100] = "changes";
    if (extract_key_changes(diff_output, specific_desc)) {
        printf("3. %s: %s\n", commit_type, specific_desc);
    }
    
    printf("\n💡 Tip: Use conventional commit format: <type>[optional scope]: <description>\n\n");
}

// ==================== CODE REVIEW HELPER ====================

void generate_review_checklist() {
    printf("🔍 Code Review Checklist\n");
    printf("=======================\n");
    
    char* diff_output = run_git_command_output("git diff HEAD~1");
    if (!diff_output || strlen(diff_output) == 0) {
        printf("No changes to review (or only one commit in repository).\n\n");
        return;
    }
    
    printf("Review the following for recent changes:\n\n");
    
    int issues_found = 0;
    
    if (strstr(diff_output, "TODO") || strstr(diff_output, "FIXME")) {
        printf("❌ TODO/FIXME comments added - consider addressing before merge\n");
        issues_found++;
    }
    
    if (strstr(diff_output, "printf(") || strstr(diff_output, "console.log") || strstr(diff_output, "print(")) {
        printf("⚠️  Debug prints found - remove before production\n");
        issues_found++;
    }
    
    if (strstr(diff_output, "password") || strstr(diff_output, "secret") || strstr(diff_output, "api_key")) {
        printf("🚨 Potential secrets in code - verify no hardcoded credentials\n");
        issues_found++;
    }
    
    if (strstr(diff_output, "//") && !strstr(diff_output, "// TODO") && !strstr(diff_output, "// FIXME")) {
        printf("💡 New comments added - verify they provide useful context\n");
        issues_found++;
    }
    
    int files_changed = 0;
    char* line = strtok(diff_output, "\n");
    while (line) {
        if (strstr(line, "diff --git")) {
            files_changed++;
        }
        line = strtok(NULL, "\n");
    }
    
    printf("\n📊 Summary: %d files changed, %d potential issues to check\n", files_changed, issues_found);
    
    if (issues_found == 0) {
        printf("✅ No obvious issues detected in automated checks\n");
    }
    
    printf("\n");
}

// ==================== SECURITY AUDIT ====================

void run_security_audit() {
    printf("🛡️  Security Audit\n");
    printf("=================\n");
    
    char* diff_output = run_git_command_output("git diff HEAD~10");
    if (!diff_output || strlen(diff_output) == 0) {
        printf("No recent changes to audit.\n\n");
        return;
    }
    
    int security_issues = 0;
    
    printf("Scanning for potential security issues...\n\n");
    
    if (strstr(diff_output, "system(") || strstr(diff_output, "exec(") || strstr(diff_output, "popen(")) {
        printf("❌ System command execution found - validate input sanitization\n");
        security_issues++;
    }
    
    if (strstr(diff_output, "strcpy(") || strstr(diff_output, "strcat(") || strstr(diff_output, "sprintf(")) {
        printf("⚠️  Unsafe string functions used - consider strncpy/strncat/snprintf\n");
        security_issues++;
    }
    
    if (strstr(diff_output, "malloc(") && !strstr(diff_output, "free(")) {
        printf("💡 Memory allocation without obvious free - check for leaks\n");
        security_issues++;
    }
    
    if (strstr(diff_output, "password") || strstr(diff_output, "secret") || strstr(diff_output, "key")) {
        printf("🔐 Security-related strings modified - verify no sensitive data exposure\n");
        security_issues++;
    }
    
    if (strstr(diff_output, "permission") || strstr(diff_output, "chmod") || strstr(diff_output, "access")) {
        printf("🔒 Permission changes detected - review access control requirements\n");
        security_issues++;
    }
    
    printf("\n");
    if (security_issues == 0) {
        printf("✅ No obvious security issues detected\n");
    } else {
        printf("🔍 Found %d potential security considerations to review\n", security_issues);
    }
    printf("\n");
}

// ==================== CHANGE IMPACT ANALYZER ====================

void analyze_change_impact(const char* target) {
    printf("📈 Change Impact Analysis: %s\n", target);
    printf("==============================\n");
    
    if (strlen(target) == 0) {
        printf("Please specify a file or function to analyze\n\n");
        return;
    }
    
    if (access(target, F_OK) == 0) {
        printf("Analyzing impact of changes to file: %s\n\n", target);
        
        char* commits = run_git_command_output("git log --oneline --follow -- \"%s\" | head -5", target);
        if (commits && strlen(commits) > 0) {
            printf("Recent changes to this file:\n");
            char* line = strtok(commits, "\n");
            while (line) {
                printf("  • %s\n", line);
                line = strtok(NULL, "\n");
            }
        }
        
        char* related = run_git_command_output("git log --name-only --oneline -- \"%s\" | grep -v \"^[a-f0-9]\" | sort | uniq -c | sort -rn | head -5", target);
        if (related && strlen(related) > 0) {
            printf("\nFrequently changed together:\n");
            char* line = strtok(related, "\n");
            while (line) {
                printf("  • %s\n", line);
                line = strtok(NULL, "\n");
            }
        }
    } else {
        printf("Analyzing impact of: %s\n", target);
        printf("(Note: This is a simple analysis. For complex projects, consider specialized tools.)\n");
    }
    
    printf("\n💡 Consider running tests after modifying this component\n\n");
}

// ==================== INTERACTIVE CONFLICT RESOLVER ====================

void interactive_conflict_resolver() {
    printf("🔄 Interactive Conflict Resolver\n");
    printf("===============================\n");
    
    char* status = run_git_command_output("git status --porcelain");
    if (!status || !strstr(status, "UU")) {
        printf("No merge conflicts detected.\n");
        printf("This helper assists when you have merge conflicts (files marked with 'UU').\n\n");
        return;
    }
    
    printf("Merge conflicts detected. Here's how to resolve them:\n\n");
    
    printf("1. Identify conflicted files:\n");
    char* line = strtok(status, "\n");
    while (line) {
        if (strstr(line, "UU")) {
            printf("   • %s\n", line + 3);
        }
        line = strtok(NULL, "\n");
    }
    
    printf("\n2. For each conflicted file:\n");
    printf("   - Open the file in your editor\n");
    printf("   - Look for <<<<<<<, =======, >>>>>>> markers\n");
    printf("   - Choose which changes to keep (ours/theirs/both)\n");
    printf("   - Remove the conflict markers and clean up the code\n");
    printf("   - Save the file\n");
    
    printf("\n3. After resolving all conflicts:\n");
    printf("   git add .\n");
    printf("   git commit\n");
    
    printf("\n4. Common tools that can help:\n");
    printf("   - git mergetool (opens configured merge tool)\n");
    printf("   - git diff (see differences)\n");
    printf("   - git log --merge (see conflicting commits)\n");
    
    printf("\n💡 Tip: Use 'git config --global merge.tool vscode' to set up VS Code as merge tool\n\n");
}

// ==================== PERFORMANCE REGRESSION DETECTOR ====================

void detect_performance_regressions() {
    printf("⚡ Performance Regression Detection\n");
    printf("==================================\n");
    
    char* large_commits = run_git_command_output("git log --oneline --numstat -20 | awk '/^[a-f0-9]/ { commit=$1 } /^[0-9]/ { added+=$1; deleted+=$2 } END { print \"Recent changes: +\" added \" -\" deleted \" lines\" }'");
    if (large_commits) {
        printf("Line changes in recent commits:\n");
        printf("  %s\n", large_commits);
    }
    
    char* large_files = run_git_command_output("git ls-tree -r -l HEAD | sort -n -k4 | tail -3");
    if (large_files && strlen(large_files) > 0) {
        printf("\nLargest files in repository (potential performance concerns):\n");
        char* line = strtok(large_files, "\n");
        while (line) {
            printf("  • %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
    
    char* complex_files = run_git_command_output("git ls-files | grep -E '\\.(c|cpp|java|py|js)$' | head -5");
    if (complex_files && strlen(complex_files) > 0) {
        printf("\nKey source files to monitor for performance:\n");
        char* line = strtok(complex_files, "\n");
        while (line) {
            printf("  • %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
    
    printf("\n🔍 Performance Monitoring Tips:\n");
    printf("• Monitor file size growth over time\n");
    printf("• Watch for large binary files in repo\n");
    printf("• Consider git-lfs for large assets\n");
    printf("• Profile hot files identified earlier\n\n");
}

// ==================== DOCUMENTATION GAP FINDER ====================

void find_documentation_gaps() {
    printf("📚 Documentation Gap Analysis\n");
    printf("============================\n");
    
    char* readme = run_git_command_output("ls README* 2>/dev/null | head -1");
    #ifdef _WIN32
    readme = run_git_command_output("dir README* 2>NUL | findstr README");
    #endif
    
    if (!readme || strlen(readme) == 0) {
        printf("❌ No README file found - consider adding project documentation\n");
    } else {
        printf("✅ README file found: %s\n", readme);
    }
    
    char* recent_code_changes = run_git_command_output("git log --oneline -10 -- \"*.c\" \"*.cpp\" \"*.java\" \"*.py\" \"*.js\"");
    char* recent_doc_changes = run_git_command_output("git log --oneline -10 -- \"*.md\" \"*.txt\" \"README*\" \"docs/*\"");
    
    if (recent_code_changes && strlen(recent_code_changes) > 0) {
        int code_commits = 0;
        char* line = strtok(recent_code_changes, "\n");
        while (line) { code_commits++; line = strtok(NULL, "\n"); }
        
        int doc_commits = 0;
        if (recent_doc_changes) {
            line = strtok(recent_doc_changes, "\n");
            while (line) { doc_commits++; line = strtok(NULL, "\n"); }
        }
        
        printf("\nRecent activity:\n");
        printf("  Code commits: %d\n", code_commits);
        printf("  Documentation commits: %d\n", doc_commits);
        
        if (doc_commits < code_commits / 3) {
            printf("⚠️  Documentation may be lagging behind code changes\n");
        }
    }
    
    printf("\n💡 Documentation Tips:\n");
    printf("• Update README when adding features\n");
    printf("• Document API changes in commit messages\n");
    printf("• Consider adding inline comments for complex logic\n");
    printf("• Keep CHANGELOG.md for release notes\n\n");
}

// ==================== MAIN COMMAND HANDLER ====================

void show_analysis() {
    printf("\n");
    printf("🎯 GitSmart Analysis Report\n");
    printf("==========================\n\n");
    
    load_commit_history();
    load_branch_info();
    load_file_analysis();
    
    show_commit_summary();
    show_branch_analysis();
    show_hot_files();
    show_cleanup_suggestions();
}

void show_help_full() {
    printf("GitSmart - Intelligent Git Repository Analysis\n");
    printf("=============================================\n");
    printf("Usage: gitsmart [COMMAND] [OPTIONS]\n");
    printf("\nCommands:\n");
    printf("  analysis    Show comprehensive repository analysis (default)\n");
    printf("  blame FILE  Show smart blame with commit context\n");
    printf("  branches    Show branch analysis and cleanup suggestions\n");
    printf("  hotfiles    Show most frequently changed files\n");
    printf("  cleanup     Show cleanup suggestions\n");
    printf("  suggest     AI-powered commit message suggestions\n");
    printf("  review      Generate code review checklist\n");
    printf("  security    Run security audit on recent changes\n");
    printf("  impact TGT  Analyze change impact for file/component\n");
    printf("  resolve     Interactive merge conflict resolver\n");
    printf("  performance Detect potential performance regressions\n");
    printf("  docs        Find documentation gaps\n");
    printf("  help        Show this help message\n");
    printf("\nExamples:\n");
    printf("  gitsmart                    # Full analysis\n");
    printf("  gitsmart suggest            # AI commit suggestions\n");
    printf("  gitsmart review             # Code review helper\n");
    printf("  gitsmart security           # Security audit\n");
    printf("  gitsmart impact src/main.c  # Change impact analysis\n");
    printf("  gitsmart resolve            # Conflict resolution helper\n");
    printf("  gitsmart performance        # Performance regression detection\n");
}

// ==================== MAIN FUNCTION ====================

int main(int argc, char *argv[]) {
    if (!is_git_repository()) {
        printf("❌ Error: Not a git repository\n");
        printf("Run this command in a git repository\n");
        return 1;
    }
    
    if (argc == 1) {
        show_analysis();
    } else if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            show_help_full();
        } else if (strcmp(argv[1], "branches") == 0) {
            load_branch_info();
            show_branch_analysis();
        } else if (strcmp(argv[1], "hotfiles") == 0) {
            load_file_analysis();
            show_hot_files();
        } else if (strcmp(argv[1], "cleanup") == 0) {
            show_cleanup_suggestions();
        } else if (strcmp(argv[1], "analysis") == 0) {
            show_analysis();
        } else if (strcmp(argv[1], "suggest") == 0) {
            generate_commit_suggestions();
        } else if (strcmp(argv[1], "review") == 0) {
            generate_review_checklist();
        } else if (strcmp(argv[1], "security") == 0) {
            run_security_audit();
        } else if (strcmp(argv[1], "resolve") == 0) {
            interactive_conflict_resolver();
        } else if (strcmp(argv[1], "performance") == 0) {
            detect_performance_regressions();
        } else if (strcmp(argv[1], "docs") == 0) {
            find_documentation_gaps();
        } else {
            printf("Unknown command: %s\n", argv[1]);
            show_help_full();
            return 1;
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "blame") == 0) {
            smart_blame(argv[2]);
        } else if (strcmp(argv[1], "impact") == 0) {
            analyze_change_impact(argv[2]);
        } else {
            printf("Unknown command: %s\n", argv[1]);
            show_help_full();
            return 1;
        }
    } else {
        show_help_full();
        return 1;
    }
    
    return 0;
}