#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>

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

    FILE *fp = popen(command, "r");
    if (!fp) {
        return -1;
    }

    int result = pclose(fp);
    return WEXITSTATUS(result);
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
    return output;
}

int is_git_repository() {
    return run_git_command("git rev-parse --git-dir > /dev/null 2>&1") == 0;
}

// ==================== COMMIT ANALYSIS ====================

void load_commit_history() {
    char *output = run_git_command_output("git log --oneline --format=%%H|%%an|%%ad|%%s --date=short -1000");
    if (!output) return;

    char *line = strtok(output, "\n");
    while (line && commit_count < MAX_COMMITS) {
        commit_info_t *commit = &commits[commit_count];
        
        char *hash = strtok(line, "|");
        char *author = strtok(NULL, "|");
        char *date = strtok(NULL, "|");
        char *message = strtok(NULL, "|");
        
        if (hash && author && date && message) {
            strncpy(commit->hash, hash, 40);
            strncpy(commit->author, author, 255);
            strncpy(commit->date, date, 63);
            strncpy(commit->message, message, 511);
            
            // Get commit stats
            char stats_cmd[256];
            snprintf(stats_cmd, sizeof(stats_cmd), "git show --stat --oneline %s | tail -1", hash);
            char *stats = run_git_command_output(stats_cmd);
            if (stats) {
                char *files = strstr(stats, "file");
                if (files) {
                    commit->files_changed = atoi(files - 3);
                }
                
                char *insertions = strstr(stats, "insertion");
                if (insertions) {
                    commit->insertions = atoi(insertions - 5);
                }
                
                char *deletions = strstr(stats, "deletion");
                if (deletions) {
                    commit->deletions = atoi(deletions - 5);
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
    printf("\n");
}

// ==================== BRANCH ANALYSIS ====================

void load_branch_info() {
    char *output = run_git_command_output("git branch -v --format=%%(refname:short)|%%(objectname:short)|%%(upstream:track)");
    if (!output) return;

    char *line = strtok(output, "\n");
    while (line && branch_count < MAX_BRANCHES) {
        branch_info_t *branch = &branches[branch_count];
        
        char *name = strtok(line, "|");
        char *commit = strtok(NULL, "|");
        char *tracking = strtok(NULL, "|");
        
        if (name && commit) {
            strncpy(branch->name, name, 255);
            strncpy(branch->last_commit, commit, 40);
            
            branch->is_merged = 0;
            if (strcmp(name, "main") != 0 && strcmp(name, "master") != 0) {
                char cmd[256];
                snprintf(cmd, sizeof(cmd), "git branch --merged main 2>/dev/null | grep -q \"^%s$\"", name);
                branch->is_merged = (run_git_command(cmd) == 0);
            }
            
            branch_count++;
        }
        line = strtok(NULL, "\n");
    }
}

void show_branch_analysis() {
    printf("🌿 Branch Analysis\n");
    printf("=================\n");
    
    int merged_branches = 0;
    int active_branches = 0;
    
    for (int i = 0; i < branch_count; i++) {
        if (branches[i].is_merged) {
            merged_branches++;
        } else if (strcmp(branches[i].name, "main") != 0 && strcmp(branches[i].name, "master") != 0) {
            active_branches++;
        }
    }
    
    printf("Total branches: %d\n", branch_count);
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
    if (!output) return;

    char *line = strtok(output, "\n");
    while (line && file_count < MAX_FILES) {
        file_info_t *file = &files[file_count];
        strncpy(file->path, line, MAX_PATH_LENGTH - 1);
        
        // Get file change frequency
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "git log --oneline --format=%%H \"%s\" | wc -l", line);
        char *changes = run_git_command_output(cmd);
        if (changes) {
            file->changes = atoi(changes);
        }
        
        // Get last author
        snprintf(cmd, sizeof(cmd), "git log -1 --format=%%an \"%s\"", line);
        char *author = run_git_command_output(cmd);
        if (author) {
            strncpy(file->last_author, author, 255);
            // Remove newline
            file->last_author[strcspn(file->last_author, "\n")] = 0;
        }
        
        file_count++;
        line = strtok(NULL, "\n");
    }
}

void show_hot_files() {
    printf("🔥 Frequently Changed Files\n");
    printf("===========================\n");
    
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
    for (int i = 0; i < count; i++) {
        printf("%3d changes: %s\n", files[i].changes, files[i].path);
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
    
    char *output = run_git_command_output("git blame -s -e \"%s\"", filepath);
    if (!output) {
        printf("Failed to get blame information\n");
        return;
    }
    
    char *line = strtok(output, "\n");
    int line_num = 1;
    int lines_shown = 0;
    
    while (line && lines_shown < 15) {
        char *hash = strtok(line, " ");
        char *author_start = strstr(line, "(");
        if (author_start) {
            author_start += 1; // Skip the '('
            char *author_end = strstr(author_start, ")");
            
            if (hash && author_start && author_end) {
                char author[256] = {0};
                strncpy(author, author_start, author_end - author_start);
                
                // Get commit message for context
                char msg_cmd[256];
                snprintf(msg_cmd, sizeof(msg_cmd), "git show -s --format=%%s %s", hash);
                char *message = run_git_command_output(msg_cmd);
                
                if (message) {
                    // Remove newline from message
                    message[strcspn(message, "\n")] = 0;
                    printf("%3d: %s - %s\n", line_num, author, message);
                    lines_shown++;
                }
            }
        }
        
        line_num++;
        line = strtok(NULL, "\n");
    }
    
    if (lines_shown == 15) {
        printf("... (showing first 15 lines)\n");
    }
    printf("\n");
}

// ==================== CLEANUP SUGGESTIONS ====================

void show_cleanup_suggestions() {
    printf("🧹 Cleanup Suggestions\n");
    printf("=====================\n");
    
    // Check for large files
    char *large_files = run_git_command_output("git ls-tree -r -l HEAD | sort -n -k4 | tail -5");
    if (large_files && strlen(large_files) > 0) {
        printf("📁 Largest files in repository:\n");
        char *line = strtok(large_files, "\n");
        while (line) {
            printf("  %s\n", line);
            line = strtok(NULL, "\n");
        }
        printf("\n");
    }
    
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
        
        if (modified > 0) printf("📝 Modified files: %d\n", modified);
        if (untracked > 0) printf("❓ Untracked files: %d\n", untracked);
    } else {
        printf("✅ Working directory is clean\n");
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