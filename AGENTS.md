# Hydration Pebble Agent Team

This repository uses a simple branch model:

- `main`: version bumps and release metadata only.
- `dev`: integration branch for validated work.
- `feature/*`: feature branches created from `dev`.

## Agent Roles

1. **Coordinator Agent**
   - Creates `feature/*` branches from `dev`.
   - Breaks work into small deliverables.
   - Opens and updates pull requests into `dev`.

2. **Companion + Data Agent**
   - Owns phone companion behavior.
   - Adds and validates intake logging persistence ("database"-style local storage).
   - Ensures watch↔phone message keys stay consistent.

3. **UX/UI Agent**
   - Improves navigation clarity and user-facing labels.
   - Keeps interaction flow easy to understand.
   - Confirms cross-platform readability (aplite + color devices).

4. **Visual Motion Agent**
   - Adds visual polish: color theme and lightweight animation.
   - Avoids battery-heavy effects.
   - Keeps rendering compatible with Pebble SDK limits.

5. **Release Agent**
   - Merges `feature/*` into `dev` after validation.
   - Promotes `dev` into `main` only for version/release bumps.

## Delivery Flow

1. Branch from `dev` (`feature/<scope>`).
2. Implement and validate changes.
3. Open PR to `dev`.
4. After acceptance, merge to `dev`.
5. Use a separate PR from `dev` to `main` for version bump/release only.
