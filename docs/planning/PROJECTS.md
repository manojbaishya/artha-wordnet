# Artha Project Board

We track work on a GitHub Project (board) attached to this repository. The board is the single source of truth for what is planned, in progress, and done.

## Board layout

**View:** Board (Kanban)  
**Group by:** Status  
**Columns (left → right):**

1. `Not Planned`
2. `Selected`
3. `In Progress`
4. `In Review`
5. `In QA`
6. `Done`

## Fields

| Field | Type | Values |
|-------|------|--------|
| Status | Single select | Not Planned, Selected, In Progress, In Review, In QA, Done |
| Issue type | Label | `epic`, `user-story`, `bug`, `task`, `sub-task` |
| Iteration | Iteration | Sprint 1, Sprint 2, … (or "Backlog") |
| Assignee | Person | Who is doing the work |
| PR | Link | The open pull request implementing the issue |

## Hierarchy

```
Epic (issue, label `epic`)
└── User Story (issue, label `user-story`)
    ├── Task (issue, label `task`)
    │   └── Sub-task (issue, label `sub-task`)
    └── Task ...
```

Use **Add sub-issue** on a parent issue to create the parent→child link. A sub-issue shows up on the parent's sidebar under "Sub-issues".

## How an item moves

| From → To | Trigger |
|-----------|---------|
| `Not Planned` → `Selected` | Sprint planning pulls the issue into the sprint |
| `Selected` → `In Progress` | Contributor starts work and opens a PR |
| `In Progress` → `In Review` | PR is ready; CI green; reviewer requested |
| `In Review` → `In QA` | Reviewer approved; verification on host / Xephyr |
| `In QA` → `Done` | PR merged to `main` |

A PR should be added to the board as an item so its Status tracks alongside its linked issue.

## Linking PRs to issues

- In the PR body: `Resolves #<issue-number>` or `closes #<issue-number>`
- In the Project board: add both the issue and the PR as items; set each item's Status
- PR title convention: `<TASK-ID>: <issue title>`
