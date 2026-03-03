# Reachability-Based Grounding in RantanPlan

The goal of grounding in automated planning is to take "lifted" action schemas that have parameters (e.g., `drive(?vehicle, ?from, ?to)`) and generate all the concrete, specific instances of these actions (e.g., `drive(truck1, cityA, cityB)`) that could *possibly* be executed while trying to solve the problem.

Replacing parameters with every combination of objects creates an astronomically huge number of actions. To avoid this, RantanPlan uses a **"reachability-based" delete-relaxation fixpoint algorithm**, which only grounds actions that could hypothetically be reached from the initial state.

Here is a step-by-step explanation of how the grounder executes this, interleaved with a concrete scenario (a simple truck-driving problem) highlighting how explicit facts and **numeric fluents** (like fuel, distance, capacity) are handled.

### The Scenario: A Truck Delivery
Imagine we have a lifted action schema: 
`drive(?vehicle, ?from, ?to)`
* **Preconditions**: `at(?vehicle, ?from)`, `connected(?from, ?to)`, `not (broken(?vehicle))`, and `fuel(?vehicle) >= 10`.
* **Effects**: Add `at(?vehicle, ?to)`, Delete `at(?vehicle, ?from)`, Decrease `fuel(?vehicle)` by `10`.

---

### Step 1: The Initial State and Closed-World Assumption
Before the loop starts, the system looks at the initial state to build a database of known facts (the **`FactIndex`**). 

* **Example Action taken**: Suppose the initial state says `at(truck1, cityA) = true`, `connected(cityA, cityB) = true`, and `fuel(truck1) = 50`.
* **The Boolean Rule**: Using the Closed-World assumption, it saves the explicit `true` facts: `at(truck1, cityA)` and `connected(cityA, cityB)`. Anything unmentioned (like `at(truck1, cityB)`) is assumed to be `false`.
* **The Numeric Rule**: **Numeric assignments are completely ignored.** The grounder skips `fuel(truck1) = 50`. Why? Because this phase is purely a logical/structural reachability check. Tracking exact numeric bounds is pushed off to a separate, later step (the Numeric Relaxed Planning Graph, or NRPG).

### Step 2: Extracting Preconditions
The grounder enters its main loop. It inspects our `drive(?vehicle, ?from, ?to)` schema to see what conditions are required to execute it.

* **Example Action taken**: It pulls out the positive boolean atoms: `at(?vehicle, ?from)` and `connected(?from, ?to)`. 
* **The Negative Rule**: It ignores `not (broken(?vehicle))` because of *delete-relaxation*. In a relaxed world, we assume negative requirements are always satisfiable because we pretend bad things never happen. 
* **The Numeric Rule**: It completely ignores the numeric precondition `fuel(?vehicle) >= 10`! Again, boolean reachability assumes that any numeric requirement can *theoretically* be met, so as not to accidentally discard an action that might actually be reachable. 

### Step 3: Finding Bindings (The "Database Join")
Now, the **`BindingMatcher`** tries to find the actual objects that match these extracted requirements by treating the `FactIndex` like SQL database tables.

* **Example Action taken**: It looks at the `at` table and sees `at(truck1, cityA)`. So it binds `?vehicle = truck1` and `?from = cityA`.
* It then looks at the `connected` table for `connected(cityA, ?to)`. It finds `connected(cityA, cityB)`, so it binds `?to = cityB`.
* If a parameter wasn't tied to a precondition (say there was a `?driver` parameter but no precondition restricted it), it just guesses every single person in the problem that fits the `driver` type. 

### Step 4: Instantiating and Deduplicating Actions
The combination of objects is grouped into a `PartialBinding`. The system checks if it has processed this exact action before.

* **Example Action taken**: The system creates a fingerprint for `drive(truck1, cityA, cityB)`. Since it is the first time seeing this combination, it instantiates the actual ground action and adds it to the master list of possible actions. 

### Step 5: Collecting "Add Effects" (Delete-Relaxation)
As soon as `drive(truck1, cityA, cityB)` is instantiated, the system checks what effects that action causes to see what *new* facts become possible.

* **Example Action taken**: The effects are: Add `at(truck1, cityB)`, Delete `at(truck1, cityA)`, Decrease `fuel(truck1)` by `10`.
* **The Delete-Relaxation Rule**: The "Delete `at(truck1, cityA)`" is ignored. In our relaxed analysis world, objects can be in multiple places at once; we only care about *accumulating* truths.
* **The Numeric Rule**: "Decrease `fuel(truck1)` by `10`" is completely bypassed. Numeric effects (increase, decrease, assign numbers) cannot produce boolean `true` facts, so they do nothing to expand our `FactIndex`.
* **The Result**: Only "Add `at(truck1, cityB)`" matters. It is successfully registered as a newly accessible fact and inserted into the `FactIndex`.

### Step 6: The Fixpoint Loop 
Because Step 5 added a *new* fact (`at(truck1, cityB)`) to the `FactIndex`, the system knows it needs to run the entire loop again. 

* **Example Action taken**: On Iteration 2, when checking preconditions, the `BindingMatcher` now sees `at(truck1, cityB)`. 
* Because of this, if `connected(cityB, cityC)` is in the `FactIndex`, it can now successfully bind and instantiate the action `drive(truck1, cityB, cityC)`.
* This action adds `at(truck1, cityC)`. It triggers Iteration 3.
* The loop continues expanding the frontier until an entire loop finishes without discovering a single new fact. This is called the **Fixpoint**.

### Step 7: Goal Reachability Check
At the end of an iteration, the grounder checks if the user's Goals are met by the current `FactIndex`. 

* **Example Action taken**: Suppose the goal is `at(truck1, cityC) AND fuel(truck1) >= 5`.
* **The Numeric Rule**: It only checks if the boolean `at(truck1, cityC)` has been reached. It skips checking `fuel(truck1) >= 5` because boolean reachability doesn't know what the numbers are.
* If `at(truck1, cityC)` never enters the `FactIndex` by the time we hit the fixpoint, the grounder returns `proven_unsolvable = true` and perfectly aborts the program early! If the truck can't get to City C even when it has infinite fuel and can be in multiple places at once, it definitely can't do it in the real constraints.