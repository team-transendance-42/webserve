 ---
  Claude CLI Power User Guide

  ---
  1. What is "Superpowers" (from GitHub)?

  Superpowers is a community plugin for Claude Code that adds a library
  of skills — reusable prompt workflows that fire before Claude responds
   to specific task types.

  - GitHub repo: Ejb503/superpowers-mcp (or similar — search
  "superpowers claude code")
  - Installed into your ~/.claude/ config, it registers a Skill tool
  Claude can call
  - Skills are markdown files with frontmatter that describe WHEN and
  HOW to use them

  You already have it — look at your session: skills like
  superpowers:brainstorming, superpowers:writing-plans,
  superpowers:test-driven-development, superpowers:systematic-debugging
  are all active.

  Why use it? It enforces discipline:
  - Before building → brainstorming (explore intent first)
  - Before coding → writing-plans (design first)
  - Before fixing → systematic-debugging (diagnose before guessing)
  - Before claiming done → verification-before-completion (prove it
  works)
  ---------------------------------------------------------
   superpowers:using-git-worktrees.

  What they are: A worktree lets you check out a second branch into a
  separate directory without disrupting your current working tree. Like
  having two project folders open simultaneously, each on a different
  branch.

  # Normal git: one directory, one branch
  /home/pskpe/webserve  (branch: main)

  # With worktrees: two directories, two branches simultaneously
  /home/pskpe/webserve          (branch: main)
  /home/pskpe/webserve-feature  (branch: feature/auth)

  Why you need them with Claude:
  - Claude can work on a feature in isolation without touching your
  active code
  - Multiple parallel agents can work on independent tasks
  simultaneously
  - Safe experimentation — the main workspace stays clean

  How to create one:
  git worktree add ../webserve-feature feature/my-feature
  # or create a new branch:
  git worktree add -b feature/new-thing ../webserve-new main

  The superpowers skill (superpowers:using-git-worktrees) automates this
   — when you ask Claude to implement something, it creates a worktree
  first, does the work there, then presents merge options.

  ---
   /plan (EnterPlanMode) puts Claude into read-only planning mode. Claude
   can read files and think, but cannot edit or run commands until you
  approve the plan.

  Why use it:
  - You see the full strategy before any code is touched
  - You can correct direction before Claude goes 200 lines deep in the
  wrong approach
  - Forces alignment on scope — prevents "feature creep" during
  implementation

  How to use:
  /plan add JWT authentication to the server
  Claude will explore the codebase, write a step-by-step plan, and wait.
   You review and say "go" or redirect.

  The full workflow (superpowers enforces this):
  /brainstorm → explore requirements and options
       ↓
  /plan → write detailed implementation plan
       ↓
  Claude executes plan in worktree
       ↓
  /review → verify against plan before merging

  --------------------

  │ Command  │                  What it does                  │
  ├──────────┼────────────────────────────────────────────────┤
  │ /plan    │ Enter read-only planning mode                  │
  ├──────────┼────────────────────────────────────────────────┤
  │ /clear   │ Clear conversation context (start fresh)       │
  ├──────────┼────────────────────────────────────────────────┤
  │ /compact │ Summarize conversation to save context         │
  ├──────────┼────────────────────────────────────────────────┤
  │ /config  │ Open settings (model, theme, etc.)             │
  ├──────────┼────────────────────────────────────────────────┤
  │ /fast    │ Toggle fast mode (Opus 4.6 with faster output) │
  ├──────────┼────────────────────────────────────────────────┤
  │ /help    │ Show all commands                              │
  ├──────────┼────────────────────────────────────────────────┤
  │ /review  │ Review a PR                                    │
  ├──────────┼────────────────────────────────────────────────┤
  │ /init    │ Generate CLAUDE.md for a new project           │
  ├──────────┼────────────────────────────────────────────────┤
  │ /memory  │ View/manage memory    































