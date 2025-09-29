# GitSmart 🧠

**Intelligent Git Repository Analysis Tool**  
_Built for CLI Line-Limit Hackathon 2025_

[![GitHub](https://img.shields.io/badge/Hackathon-CLI%20Line--Limit%202025-blue)](https://example.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-green.svg)](https://example.com)

A powerful CLI tool that provides intelligent analysis of Git repositories with AI-powered insights, security auditing, and developer workflow enhancements - all within strict line constraints.

---

## 🚀 Features

### Core Analysis

- **📊 Repository Overview** — Commit history, author activity, change statistics
- **🌿 Branch Intelligence** — Identify stale branches, merge suggestions
- **🔥 Hot File Detection** — Most frequently changed files
- **🧹 Cleanup Suggestions** — Working directory status and optimization tips

### AI-Powered Insights

- **🤖 Smart Commit Suggestions** — AI-generated commit messages based on staged changes
- **🔍 Code Review Helper** — Automated checklist for code reviews
- **📈 Change Impact Analysis** — Understand how changes affect your codebase

### Security & Quality

- **🛡️ Security Audit** — Detect potential security issues in recent changes
- **⚡ Performance Regression Detection** — Identify potential performance concerns
- **📚 Documentation Gap Finder** — Keep docs in sync with code changes

### Developer Experience

- **🔍 Smart Blame** — Enhanced git blame with commit context
- **🔄 Conflict Resolver** — Interactive merge conflict guidance
- **💡 Workflow Optimization** — Suggestions for better Git practices

---

## 🛠️ Installation

### Prerequisites

- Git installed and in PATH
- C compiler (GCC, Clang, or MSVC)

### Build from Source

#### Linux/macOS

```bash
# Clone the repository
git clone https://github.com/yourusername/gitsmart.git
cd gitsmart

# Compile
gcc -o gitsmart main.c

# Install (optional)
sudo cp gitsmart /usr/local/bin/
```

#### Windows

```cmd
gcc -o gitsmart.exe main.c
```

---

## 📖 Usage

Run from any Git repository:

```bash
# Comprehensive analysis
./gitsmart

# Specific commands
./gitsmart analysis          # Full repository analysis
./gitsmart branches          # Branch analysis and cleanup
./gitsmart hotfiles          # Most frequently changed files
./gitsmart blame <file>      # Smart blame with context
./gitsmart suggest           # AI commit message suggestions
./gitsmart review            # Code review checklist
./gitsmart security          # Security audit
./gitsmart impact <target>   # Change impact analysis
./gitsmart resolve           # Conflict resolution helper
./gitsmart performance       # Performance regression detection
./gitsmart docs              # Documentation gap analysis
./gitsmart help              # Show full help
```

---

## 🎯 Examples

### Daily Development Workflow

```bash
# After making changes
git add .
./gitsmart suggest           # Get AI commit message suggestions
./gitsmart review            # Review your changes before committing

# Before pushing
./gitsmart analysis          # Full repository health check
./gitsmart security          # Security audit
```

### Team Collaboration

```bash
# Reviewing PRs
./gitsmart impact src/main.c # Understand change impact
./gitsmart hotfiles          # See what's changing frequently

# Repository maintenance
./gitsmart branches          # Clean up merged branches
./gitsmart cleanup           # General cleanup suggestions
```

---

## 📊 Sample Output

```text
🎯 GitSmart Analysis Report
==========================

📊 Repository Analysis
=====================
Total commits: 142
Total changes: +8452 -3124 lines
Most active author: Alice (89 commits)
Latest commit: 2024-09-29 by Bob

🌿 Branch Analysis
=================
Total branches: 7
Current branch: feature/new-auth
Active branches: 3
Merged branches (can be deleted): 2

🔥 Frequently Changed Files
===========================
Top 5 most frequently changed files:
 45 changes: src/auth.js (last by: Charlie)
 32 changes: package.json (last by: Alice)
 28 changes: src/main.c (last by: Bob)
...
```

---

## 🏗️ Architecture

GitSmart is built with:

- **Cross-platform C** — Runs anywhere Git runs
- **Minimal dependencies** — Only requires standard C library and Git
- **Efficient parsing** — Smart Git command execution and output processing
- **Modular design** — Clean separation of analysis features

---

## 🎪 Hackathon Compliance

### Line Count Discipline

- **Language:** C
- **Line Limit:** 1000 executable lines
- **Current Status:** ✅ Within limits

### Code Quality

- Clean, readable, idiomatic C

### Key Constraints Met

- ✅ Exact line limit adherence
- ✅ No padding or compression tricks
- ✅ Clean, maintainable code
- ✅ Comprehensive error handling
- ✅ Cross-platform compatibility
- ✅ Practical daily utility
