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

    // Windows-specific command hiding
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
    
    // Remove trailing newline if present
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
    // Use simpler format that works on both Windows and Unix
    char *output = run_git_command_output("git log --oneline --format=\"%%H %%an %%ad %%s\" --date=short -1000");
    if (!output) {
        printf("Failed to get commit history\n");
        return;
    }

    char *line = strtok(output, "\n");
    while (line && commit_count < MAX_COMMITS) {
        commit_info_t *commit = &commits[commit_count];
        
        // Parse the line: hash author date message
        char *hash = strtok(line, " ");
        char *author = strtok(NULL, " ");
        char *date = strtok(NULL, " ");
        char *message = strtok(NULL, "\n"); // Rest of the line is message
        
        if (hash && author && date && message) {
            // Hash is first 7 characters (short hash)
            strncpy(commit->hash, hash, 40);
            commit->hash[40] = '\0';
            
            strncpy(commit->author, author, 255);
            commit->author[255] = '\0';
            
            strncpy(commit->date, date, 63);
            commit->date[63] = '\0';
            
            strncpy(commit->message, message, 511);
            commit->message[511] = '\0';
            
            // Get detailed commit info for stats
            char stats_cmd[512];
            snprintf(stats_cmd, sizeof(stats_cmd), "git show --stat %s", hash);
            char *stats_output = run_git_command_output(stats_cmd);
            
            if (stats_output) {
                // Reset stats
                commit->files_changed = 0;
                commit->insertions = 0;
                commit->deletions = 0;
                
                // Parse stats from the output
                char *stat_line = strtok(stats_output, "\n");
                while (stat_line) {
                    // Look for the summary line at the end
                    if (strstr(stat_line, "file") != NULL) {
                        // Try to parse: " 2 files changed, 15 insertions(+), 3 deletions(-)"
                        char *files_ptr = strstr(stat_line, "file");
                        if (files_ptr) {
                            // Go backwards to find the number
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
    
    // Most active author
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
    
    // Find top author
    int max_commits = 0;
    char top_author[256] = "";
    for (int i = 0; i < author_count; i++) {
        if (author_counts[i] > max_commits) {
            max_commits = author_counts[i];
            strcpy(top_author, authors[i]);
        }
    }
    
    printf("Most active author: %s (%d commits)\n", top_author, max_commits);
    
    // Recent activity
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
        
        // Skip leading spaces and asterisk (current branch)
        const char *ptr = line;
        while (*ptr == ' ' || *ptr == '*') ptr++;
        
        // Branch name ends at space or end of string
        const char *name_end = ptr;
        while (*name_end && *name_end != ' ') name_end++;
        
        if (name_end > ptr) {
            size_t name_len = name_end - ptr;
            strncpy(branch->name, ptr, name_len);
            branch->name[name_len] = '\0';
            
            // Find commit hash (skip to next word after branch name)
            const char *commit_ptr = name_end;
            while (*commit_ptr && *commit_ptr == ' ') commit_ptr++;
            
            // Commit hash is typically 7 characters (short)
            if (strlen(commit_ptr) >= 7) {
                strncpy(branch->last_commit, commit_ptr, 7);
                branch->last_commit[7] = '\0';
            }
            
            // Check if merged into main/master
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
    
    // Get current branch
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
        
        // Get file change frequency using simpler method
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "git log --oneline -- \"%s\" | find /c /v \"\"", line);
        #ifndef _WIN32
        snprintf(cmd, sizeof(cmd), "git log --oneline -- \"%s\" | wc -l", line);
        #endif
        
        char *changes_output = run_git_command_output(cmd);
        if (changes_output) {
            file->changes = atoi(changes_output);
        } else {
            file->changes = 1; // Default to 1 if we can't determine
        }
        
        // Get last author
        snprintf(cmd, sizeof(cmd), "git log -1 --format=%%an -- \"%s\"", line);
        char *author = run_git_command_output(cmd);
        if (author) {
            strncpy(file->last_author, author, 255);
            file->last_author[255] = '\0';
            // Remove newline if present
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
    
    // Sort files by change frequency (simple bubble sort)
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
    
    // Use simpler blame format
    char *output = run_git_command_output("git blame -s \"%s\"", filepath);
    if (!output) {
        printf("Failed to get blame information\n");
        return;
    }
    
    char *line = strtok(output, "\n");
    int line_num = 1;
    int lines_shown = 0;
    
    while (line && lines_shown < 10) { // Show fewer lines for clarity
        // Format: hash (author date time line_number) content
        char *hash = line;
        
        // Find the author (after parentheses)
        char *author_start = strchr(line, '(');
        if (author_start) {
            author_start++; // Skip '('
            char *author_end = strchr(author_start, ' ');
            if (author_end) {
                char author[256] = {0};
                strncpy(author, author_start, author_end - author_start);
                author[author_end - author_start] = '\0';
                
                // Get commit message
                char msg_cmd[256];
                snprintf(msg_cmd, sizeof(msg_cmd), "git show -s --format=%%s %s", hash);
                char *message = run_git_command_output(msg_cmd);
                
                if (message) {
                    // Remove newline
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
    
    // Check git status
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
    
    // Check for stashed changes
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
    printf("  help        Show this help message\n");
    printf("\nExamples:\n");
    printf("  gitsmart                    # Full analysis\n");
    printf("  gitsmart blame src/main.c   # Smart blame\n");
    printf("  gitsmart branches           # Branch analysis\n");
    printf("  gitsmart hotfiles           # Hot files analysis\n");
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
        } else {
            printf("Unknown command: %s\n", argv[1]);
            show_help_full();
            return 1;
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "blame") == 0) {
            smart_blame(argv[2]);
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