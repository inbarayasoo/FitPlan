// FitPlan front-end: auth flow and routing between the two role views.

// --- helpers -------------------------------------------------------
function showToast(message, isError) {
  const toast = document.getElementById("toast");
  toast.textContent = message;
  toast.classList.toggle("error", Boolean(isError));
  toast.classList.remove("hidden");
  window.setTimeout(() => toast.classList.add("hidden"), 4000);
}

// --- view switching ---------------------------------------------
function showAuthView() {
  document.getElementById("auth-view").classList.remove("hidden");
  document.getElementById("coach-view").classList.add("hidden");
  document.getElementById("trainee-view").classList.add("hidden");
  document.getElementById("logout-btn").classList.add("hidden");
  resetGoogleRolePrompt();
  setupGoogleSignIn();
}

function selectCoachPanel(name) {
  document.querySelectorAll("#coach-view .dash-tab").forEach((tab) => {
    tab.classList.toggle("active", tab.dataset.panel === name);
  });
  document.querySelectorAll("#coach-view .panel").forEach((panel) => {
    panel.classList.toggle("hidden", panel.id !== "panel-" + name);
  });
  if (name === "exercises") {
    loadExercises();
  } else if (name === "trainees") {
    loadTrainees();
  } else if (name === "plans") {
    loadPlansPanel();
  }
}

function showDashboard(user) {
  document.getElementById("auth-view").classList.add("hidden");
  document.getElementById("logout-btn").classList.remove("hidden");

  const isCoach = user.role === "coach";
  document.getElementById("coach-view").classList.toggle("hidden", !isCoach);
  document.getElementById("trainee-view").classList.toggle("hidden", isCoach);

  if (isCoach) {
    selectCoachPanel("exercises");
  } else {
    selectTraineePanel("my-plan");
  }
}

function selectTraineePanel(name) {
  document.querySelectorAll("#trainee-view .dash-tab").forEach((tab) => {
    tab.classList.toggle("active", tab.dataset.panel === name);
  });
  document.querySelectorAll("#trainee-view .panel").forEach((panel) => {
    panel.classList.toggle("hidden", panel.id !== "panel-" + name);
  });
  if (name === "my-plan") {
    loadMyPlan();
  } else if (name === "log-session") {
    loadLogSession();
  } else if (name === "notes") {
    loadNotes();
  } else if (name === "my-progress") {
    loadMyProgress();
  }
}

// --- session --------------------------------------------------
function onAuthSuccess(data) {
  api.setToken(data.access_token);
  api.setUser(data.user);
  showDashboard(data.user);
}

function logout() {
  api.clearToken();
  api.clearUser();
  showAuthView();
}

// --- auth forms ----------------------------------------------
async function handleLogin(event) {
  event.preventDefault();
  const form = event.target;
  const credentials = {
    email: form.elements.email.value,
    password: form.elements.password.value,
  };
  try {
    const data = await api.post("/api/auth/login", credentials);
    onAuthSuccess(data);
  } catch (err) {
    showToast(err.detail || err.title || "Login failed", true);
  }
}

async function handleRegister(event) {
  event.preventDefault();
  const form = event.target;
  const payload = {
    display_name: form.elements.display_name.value,
    email: form.elements.email.value,
    password: form.elements.password.value,
    role: form.elements.role.value,
  };
  try {
    const data = await api.post("/api/auth/register", payload);
    onAuthSuccess(data);
  } catch (err) {
    showToast(err.detail || err.title || "Registration failed", true);
  }
}

// --- Google sign-in --------------------------------------------
// The one external script in the app. It is fetched only when the server
// reports a client id, and only the first time the auth view is shown.
let googleClientId = null;
let googleReady = false;
// Holds the Google credential between the first "needs a role" reply and the
// user picking one, so the second request can re-send the same token.
let pendingGoogleToken = null;

async function loadAuthConfig() {
  try {
    const config = await api.get("/api/auth/config");
    googleClientId = config.google_client_id || null;
  } catch (err) {
    googleClientId = null;
  }
}

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = src;
    script.async = true;
    script.defer = true;
    script.onload = resolve;
    script.onerror = () => reject(new Error("failed to load " + src));
    document.head.appendChild(script);
  });
}

async function setupGoogleSignIn() {
  if (!googleClientId || googleReady) {
    return;
  }
  try {
    await loadScript("https://accounts.google.com/gsi/client");
    google.accounts.id.initialize({
      client_id: googleClientId,
      callback: handleGoogleCredential,
    });
    google.accounts.id.renderButton(document.getElementById("google-button"), {
      theme: "outline",
      size: "large",
      text: "continue_with",
      width: 260,
    });
    document.getElementById("google-signin").classList.remove("hidden");
    googleReady = true;
  } catch (err) {
    // Script blocked or offline: leave the button hidden, password login still works.
  }
}

async function handleGoogleCredential(response) {
  try {
    const data = await api.post("/api/auth/google", {
      id_token: response.credential,
    });
    if (data && data.needs_role) {
      // First sign-in for a new account: ask trainee or coach, then retry.
      pendingGoogleToken = response.credential;
      document.getElementById("google-signin").classList.add("hidden");
      document.getElementById("google-role").classList.remove("hidden");
      return;
    }
    onAuthSuccess(data);
  } catch (err) {
    showToast(err.detail || err.title || "Google sign-in failed", true);
  }
}

async function submitGoogleRole(role) {
  try {
    const data = await api.post("/api/auth/google", {
      id_token: pendingGoogleToken,
      role: role,
    });
    resetGoogleRolePrompt();
    onAuthSuccess(data);
  } catch (err) {
    showToast(err.detail || err.title || "Google sign-in failed", true);
  }
}

function resetGoogleRolePrompt() {
  pendingGoogleToken = null;
  document.getElementById("google-role").classList.add("hidden");
  document.getElementById("google-signin").classList.toggle("hidden", !googleReady);
}

// --- coach: exercises -------------------------------------------
let editingExerciseId = null;

async function loadExercises() {
  try {
    const data = await api.get("/api/exercises");
    renderExercises(data.exercises);
  } catch (err) {
    showToast(err.detail || err.title || "Could not load exercises", true);
  }
}

function renderExercises(items) {
  const list = document.getElementById("exercise-list");
  list.textContent = "";

  if (items.length === 0) {
    const empty = document.createElement("li");
    empty.className = "item-empty";
    empty.textContent = "No exercises yet.";
    list.appendChild(empty);
    return;
  }

  items.forEach((item) => {
    const li = document.createElement("li");
    li.className = "item";

    const title = document.createElement("span");
    title.className = "item-title";
    title.textContent = item.name;
    li.appendChild(title);

    if (item.category) {
      const tag = document.createElement("span");
      tag.className = "item-tag";
      tag.textContent = item.category;
      li.appendChild(tag);
    }

    const actions = document.createElement("span");
    actions.className = "item-actions";

    const editBtn = document.createElement("button");
    editBtn.type = "button";
    editBtn.className = "ghost";
    editBtn.textContent = "Edit";
    editBtn.addEventListener("click", () => fillExerciseForm(item));
    actions.appendChild(editBtn);

    const deleteBtn = document.createElement("button");
    deleteBtn.type = "button";
    deleteBtn.className = "ghost";
    deleteBtn.textContent = "Delete";
    deleteBtn.addEventListener("click", () => handleExerciseDelete(item));
    actions.appendChild(deleteBtn);

    li.appendChild(actions);
    list.appendChild(li);
  });
}

function fillExerciseForm(item) {
  const form = document.getElementById("exercise-form");
  form.elements.name.value = item.name;
  form.elements.category.value = item.category || "";
  form.elements.primary_muscle.value = item.primary_muscle || "";
  form.elements.description.value = item.description || "";
  form.elements.video_url.value = item.video_url || "";
  editingExerciseId = item.id;
  document.getElementById("exercise-submit").textContent = "Save changes";
  document.getElementById("exercise-cancel").classList.remove("hidden");
}

function resetExerciseForm() {
  document.getElementById("exercise-form").reset();
  editingExerciseId = null;
  document.getElementById("exercise-submit").textContent = "Add exercise";
  document.getElementById("exercise-cancel").classList.add("hidden");
}

async function handleExerciseSubmit(event) {
  event.preventDefault();
  const form = event.target;

  const payload = { name: form.elements.name.value.trim() };
  const category = form.elements.category.value.trim();
  const primaryMuscle = form.elements.primary_muscle.value.trim();
  const description = form.elements.description.value.trim();
  const videoUrl = form.elements.video_url.value.trim();
  if (category) payload.category = category;
  if (primaryMuscle) payload.primary_muscle = primaryMuscle;
  if (description) payload.description = description;
  if (videoUrl) payload.video_url = videoUrl;

  try {
    if (editingExerciseId) {
      await api.put("/api/exercises/" + editingExerciseId, payload);
      showToast("Exercise updated");
    } else {
      await api.post("/api/exercises", payload);
      showToast("Exercise added");
    }
    resetExerciseForm();
    loadExercises();
  } catch (err) {
    showToast(err.detail || err.title || "Could not save exercise", true);
  }
}

async function handleExerciseDelete(item) {
  if (!window.confirm('Delete "' + item.name + '"?')) {
    return;
  }
  try {
    await api.del("/api/exercises/" + item.id);
    showToast("Exercise deleted");
    if (editingExerciseId === item.id) {
      resetExerciseForm();
    }
    loadExercises();
  } catch (err) {
    showToast(err.detail || err.title || "Could not delete exercise", true);
  }
}

// --- coach: trainees ------------------------------------------
async function loadTrainees() {
  try {
    const data = await api.get("/api/trainees");
    renderTrainees(data.trainees);
  } catch (err) {
    showToast(err.detail || err.title || "Could not load trainees", true);
  }
}

function renderTrainees(items) {
  const list = document.getElementById("trainee-list");
  list.textContent = "";

  if (items.length === 0) {
    const empty = document.createElement("li");
    empty.className = "item-empty";
    empty.textContent = "No trainees on your roster yet.";
    list.appendChild(empty);
    return;
  }

  items.forEach((item) => {
    const li = document.createElement("li");
    li.className = "item";

    const name = document.createElement("span");
    name.className = "item-title";
    name.textContent = item.display_name;
    li.appendChild(name);

    const email = document.createElement("span");
    email.className = "item-sub";
    email.textContent = item.email;
    li.appendChild(email);

    const actions = document.createElement("span");
    actions.className = "item-actions";

    const removeBtn = document.createElement("button");
    removeBtn.type = "button";
    removeBtn.className = "ghost";
    removeBtn.textContent = "Remove";
    removeBtn.addEventListener("click", () => handleTraineeRemove(item));
    actions.appendChild(removeBtn);

    li.appendChild(actions);
    list.appendChild(li);
  });
}

async function handleTraineeRemove(item) {
  if (!window.confirm("Remove " + item.display_name + " from your roster?")) {
    return;
  }
  try {
    await api.del("/api/trainees/" + item.id);
    showToast("Trainee removed from roster");
    loadTrainees();
  } catch (err) {
    showToast(err.detail || err.title || "Could not remove trainee", true);
  }
}

async function handleTraineeSubmit(event) {
  event.preventDefault();
  const form = event.target;
  const email = form.elements.email.value.trim();
  if (!email) {
    return;
  }
  try {
    await api.post("/api/trainees", { email: email });
    showToast("Trainee added to roster");
    form.reset();
    loadTrainees();
  } catch (err) {
    showToast(err.detail || err.title || "Could not add trainee", true);
  }
}

// --- coach: plans --------------------------------------------
let planExercises = [];  // the coach's library, reused for every item dropdown
let editingPlanId = null;
let shownPlanId = null;  // the plan whose details are currently expanded, if any

async function loadPlansPanel() {
  hidePlanDetail();
  try {
    const [trainees, exercises, plans] = await Promise.all([
      api.get("/api/trainees"),
      api.get("/api/exercises"),
      api.get("/api/plans"),
    ]);
    planExercises = exercises.exercises;
    fillTraineeSelect(trainees.trainees);
    renderPlans(plans.plans, trainees.trainees);
    resetPlanForm();
  } catch (err) {
    showToast(err.detail || err.title || "Could not load plans", true);
  }
}

function fillTraineeSelect(trainees) {
  const select = document.getElementById("plan-form").elements.trainee_id;
  select.textContent = "";
  trainees.forEach((trainee) => {
    const option = document.createElement("option");
    option.value = String(trainee.id);
    option.textContent = trainee.display_name + " (" + trainee.email + ")";
    select.appendChild(option);
  });
}

function makeExerciseSelect() {
  const select = document.createElement("select");
  select.className = "plan-item-exercise";
  select.required = true;
  planExercises.forEach((exercise) => {
    const option = document.createElement("option");
    option.value = String(exercise.id);
    option.textContent = exercise.name;
    select.appendChild(option);
  });
  return select;
}

function addPlanItemRow(item) {
  const row = document.createElement("div");
  row.className = "plan-item-row";

  const day = document.createElement("input");
  day.type = "text";
  day.className = "plan-item-day";
  day.placeholder = "Day";
  if (item && item.day_label) day.value = item.day_label;
  row.appendChild(day);

  const exerciseSelect = makeExerciseSelect();
  if (item) {
    exerciseSelect.value = String(item.exercise_id);
  }
  row.appendChild(exerciseSelect);

  const sets = document.createElement("input");
  sets.type = "number";
  sets.className = "plan-item-sets";
  sets.min = "1";
  sets.placeholder = "Sets";
  if (item && item.target_sets != null) sets.value = item.target_sets;
  row.appendChild(sets);

  const reps = document.createElement("input");
  reps.type = "number";
  reps.className = "plan-item-reps";
  reps.min = "1";
  reps.placeholder = "Reps";
  if (item && item.target_reps != null) reps.value = item.target_reps;
  row.appendChild(reps);

  const weight = document.createElement("input");
  weight.type = "number";
  weight.className = "plan-item-weight";
  weight.min = "0";
  weight.step = "0.5";
  weight.placeholder = "Weight";
  if (item && item.target_weight != null) weight.value = item.target_weight;
  row.appendChild(weight);

  const video = document.createElement("input");
  video.type = "url";
  video.className = "plan-item-video";
  video.placeholder = "Tutorial URL (optional)";
  if (item && item.video_url) video.value = item.video_url;
  row.appendChild(video);

  const remove = document.createElement("button");
  remove.type = "button";
  remove.className = "ghost plan-item-remove";
  remove.textContent = "×";
  remove.addEventListener("click", () => row.remove());
  row.appendChild(remove);

  document.getElementById("plan-items").appendChild(row);
}

function resetPlanForm() {
  document.getElementById("plan-form").reset();
  document.getElementById("plan-items").textContent = "";
  addPlanItemRow();
  editingPlanId = null;
  document.getElementById("plan-submit").textContent = "Create plan";
  document.getElementById("plan-cancel").classList.add("hidden");
}

function fillPlanForm(plan) {
  const form = document.getElementById("plan-form");
  form.elements.trainee_id.value = String(plan.trainee_id);
  form.elements.name.value = plan.name;
  form.elements.notes.value = plan.notes || "";

  document.getElementById("plan-items").textContent = "";
  plan.items.forEach((item) => addPlanItemRow(item));

  editingPlanId = plan.id;
  document.getElementById("plan-submit").textContent = "Save changes";
  document.getElementById("plan-cancel").classList.remove("hidden");
  form.scrollIntoView();
}

function readPlanItems() {
  const rows = document.querySelectorAll("#plan-items .plan-item-row");
  const items = [];
  rows.forEach((row) => {
    const item = {
      exercise_id: Number(row.querySelector(".plan-item-exercise").value),
    };
    const day = row.querySelector(".plan-item-day").value.trim();
    const sets = row.querySelector(".plan-item-sets").value;
    const reps = row.querySelector(".plan-item-reps").value;
    const weight = row.querySelector(".plan-item-weight").value;
    const video = row.querySelector(".plan-item-video").value.trim();
    if (day) item.day_label = day;
    if (sets) item.target_sets = Number(sets);
    if (reps) item.target_reps = Number(reps);
    if (weight) item.target_weight = Number(weight);
    if (video) item.video_url = video;
    items.push(item);
  });
  return items;
}

async function handlePlanSubmit(event) {
  event.preventDefault();
  const form = event.target;

  const items = readPlanItems();
  if (items.length === 0) {
    showToast("Add at least one exercise", true);
    return;
  }

  const payload = {
    trainee_id: Number(form.elements.trainee_id.value),
    name: form.elements.name.value.trim(),
    items: items,
  };
  const notes = form.elements.notes.value.trim();
  if (notes) payload.notes = notes;

  try {
    if (editingPlanId) {
      await api.put("/api/plans/" + editingPlanId, payload);
      showToast("Plan updated");
    } else {
      await api.post("/api/plans", payload);
      showToast("Plan created");
    }
    hidePlanDetail();
    loadPlansPanel();
  } catch (err) {
    showToast(err.detail || err.title || "Could not save plan", true);
  }
}

function renderPlans(plans, trainees) {
  const list = document.getElementById("plan-list");
  list.textContent = "";

  if (plans.length === 0) {
    const empty = document.createElement("li");
    empty.className = "item-empty";
    empty.textContent = "No plans yet.";
    list.appendChild(empty);
    return;
  }

  const nameById = {};
  trainees.forEach((trainee) => {
    nameById[trainee.id] = trainee.display_name;
  });

  plans.forEach((plan) => {
    const li = document.createElement("li");
    li.className = "item";

    const title = document.createElement("span");
    title.className = "item-title";
    title.textContent = plan.name;
    li.appendChild(title);

    const who = document.createElement("span");
    who.className = "item-sub";
    who.textContent =
      "for " + (nameById[plan.trainee_id] || "trainee #" + plan.trainee_id);
    li.appendChild(who);

    if (plan.is_active) {
      const badge = document.createElement("span");
      badge.className = "item-tag";
      badge.textContent = "active";
      li.appendChild(badge);
    }

    const actions = document.createElement("span");
    actions.className = "item-actions";

    const viewBtn = document.createElement("button");
    viewBtn.type = "button";
    viewBtn.className = "ghost";
    viewBtn.textContent = "View";
    viewBtn.addEventListener("click", () => showPlanDetail(plan.id));
    actions.appendChild(viewBtn);

    const editBtn = document.createElement("button");
    editBtn.type = "button";
    editBtn.className = "ghost";
    editBtn.textContent = "Edit";
    editBtn.addEventListener("click", () => startPlanEdit(plan.id));
    actions.appendChild(editBtn);

    if (!plan.is_active) {
      const assignBtn = document.createElement("button");
      assignBtn.type = "button";
      assignBtn.className = "ghost";
      assignBtn.textContent = "Assign";
      assignBtn.addEventListener("click", () => handlePlanAssign(plan));
      actions.appendChild(assignBtn);
    }

    const deleteBtn = document.createElement("button");
    deleteBtn.type = "button";
    deleteBtn.className = "ghost";
    deleteBtn.textContent = "Delete";
    deleteBtn.addEventListener("click", () => handlePlanDelete(plan));
    actions.appendChild(deleteBtn);

    li.appendChild(actions);
    list.appendChild(li);
  });
}

function exerciseName(id) {
  const found = planExercises.find((exercise) => exercise.id === id);
  return found ? found.name : "Exercise #" + id;
}

async function showPlanDetail(planId) {
  if (shownPlanId === planId) {
    hidePlanDetail();  // a second click on the same plan closes it
    return;
  }
  try {
    const plan = await api.get("/api/plans/" + planId);
    const box = document.getElementById("plan-detail");
    box.textContent = "";

    const heading = document.createElement("h3");
    heading.textContent = plan.name;
    box.appendChild(heading);

    const ul = document.createElement("ul");
    plan.items.forEach((item) => {
      const li = document.createElement("li");
      const parts = [];
      if (item.day_label) parts.push("[" + item.day_label + "]");
      parts.push(item.exercise_name || exerciseName(item.exercise_id));
      if (item.target_sets != null && item.target_reps != null) {
        parts.push(item.target_sets + " × " + item.target_reps);
      }
      if (item.target_weight != null) {
        parts.push(item.target_weight + " kg");
      }
      li.textContent = parts.join("   ·   ");
      if (item.video_url) {
        const a = document.createElement("a");
        a.className = "tutorial-link";
        a.textContent = "  tutorial ↗";
        a.href = item.video_url;
        a.target = "_blank";
        a.rel = "noopener noreferrer";
        li.appendChild(a);
      }
      ul.appendChild(li);
    });
    box.appendChild(ul);

    box.classList.remove("hidden");
    shownPlanId = planId;
  } catch (err) {
    showToast(err.detail || err.title || "Could not load the plan", true);
  }
}

function hidePlanDetail() {
  const box = document.getElementById("plan-detail");
  box.textContent = "";
  box.classList.add("hidden");
  shownPlanId = null;
}

async function startPlanEdit(planId) {
  try {
    const plan = await api.get("/api/plans/" + planId);
    fillPlanForm(plan);
  } catch (err) {
    showToast(err.detail || err.title || "Could not load the plan", true);
  }
}

async function handlePlanAssign(plan) {
  try {
    await api.post("/api/plans/" + plan.id + "/assign");
    showToast("Plan assigned");
    loadPlansPanel();
  } catch (err) {
    showToast(err.detail || err.title || "Could not assign plan", true);
  }
}

async function handlePlanDelete(plan) {
  if (!window.confirm('Delete the plan "' + plan.name + '"?')) {
    return;
  }
  try {
    await api.del("/api/plans/" + plan.id);
    showToast("Plan deleted");
    if (editingPlanId === plan.id) {
      resetPlanForm();
    }
    hidePlanDetail();
    loadPlansPanel();
  } catch (err) {
    showToast(err.detail || err.title || "Could not delete plan", true);
  }
}

// --- trainee: my plan ----------------------------------------
async function loadMyPlan() {
  const panel = document.getElementById("panel-my-plan");
  try {
    const plan = await api.get("/api/my/plan");
    panel.textContent = "";
    renderMyPlan(panel, plan);
  } catch (err) {
    if (err.status === 404) {
      panel.textContent = "";
      const note = document.createElement("p");
      note.className = "panel-placeholder";
      note.textContent = "No plan has been assigned to you yet.";
      panel.appendChild(note);
      return;
    }
    showToast(err.detail || err.title || "Could not load your plan", true);
  }
}

function renderMyPlan(panel, plan) {
  panel.textContent = "";

  const heading = document.createElement("h2");
  heading.textContent = plan.name;
  panel.appendChild(heading);

  if (plan.notes) {
    const notes = document.createElement("p");
    notes.className = "item-sub";
    notes.textContent = plan.notes;
    panel.appendChild(notes);
  }

  // Group the exercises by their day label, keeping first-seen order. Items
  // with no label fall together under a single "Workout" group, so a plan
  // that does not use days still shows as one plain list.
  const order = [];
  const byDay = {};
  plan.items.forEach((item) => {
    const label = item.day_label || "Workout";
    if (!byDay[label]) {
      byDay[label] = [];
      order.push(label);
    }
    byDay[label].push(item);
  });

  order.forEach((label) => {
    if (order.length > 1 || label !== "Workout") {
      const dayHeading = document.createElement("h3");
      dayHeading.textContent = label;
      panel.appendChild(dayHeading);
    }

    const list = document.createElement("ul");
    list.className = "item-list";
    byDay[label].forEach((item) => list.appendChild(myPlanItemRow(item)));
    panel.appendChild(list);
  });
}

function myPlanItemRow(item) {
  const li = document.createElement("li");
  li.className = "item my-plan-item";

  const main = document.createElement("div");
  main.className = "my-plan-item-main";

  const title = document.createElement("span");
  title.className = "item-title";
  title.textContent = item.exercise_name;
  main.appendChild(title);

  const target = document.createElement("span");
  target.className = "item-sub";
  const bits = [];
  if (item.target_sets != null && item.target_reps != null) {
    bits.push(item.target_sets + " × " + item.target_reps);
  }
  if (item.target_weight != null) bits.push(item.target_weight + " kg");
  if (item.rest_seconds != null) bits.push(item.rest_seconds + "s rest");
  target.textContent = bits.join("   ·   ");
  main.appendChild(target);

  if (item.video_url) {
    main.appendChild(tutorialControl(item, li));
  }

  li.appendChild(main);
  return li;
}

// A YouTube link (the server gives us a youtube-nocookie embed URL) toggles an
// inline iframe; anything else (Instagram, ...) opens in a new tab.
function tutorialControl(item, li) {
  if (item.video_embed_url) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "ghost";
    btn.textContent = "Watch tutorial";
    btn.addEventListener("click", () => toggleTutorialEmbed(item, li, btn));
    return btn;
  }
  const link = document.createElement("a");
  link.className = "tutorial-link";
  link.textContent = "Watch tutorial ↗";
  link.href = item.video_url;              // property, never innerHTML
  link.target = "_blank";
  link.rel = "noopener noreferrer";
  return link;
}

function toggleTutorialEmbed(item, li, btn) {
  const open = li.querySelector(".tutorial-embed");
  if (open) {
    open.remove();
    btn.textContent = "Watch tutorial";
    return;
  }
  const wrap = document.createElement("div");
  wrap.className = "tutorial-embed";
  const iframe = document.createElement("iframe");
  iframe.src = item.video_embed_url;       // server-built youtube-nocookie URL
  iframe.title = "Tutorial video";
  iframe.allow =
    "accelerometer; encrypted-media; gyroscope; picture-in-picture";
  iframe.allowFullscreen = true;
  wrap.appendChild(iframe);
  li.appendChild(wrap);
  btn.textContent = "Hide tutorial";
}

// --- trainee: log session ------------------------------------
let logPlan = null;          // the trainee's active plan, cached while the tab is open
let editingSessionId = null; // set while an existing session is being edited

async function loadLogSession() {
  const panel = document.getElementById("panel-log-session");
  const form = document.getElementById("session-form");
  const oldNote = document.getElementById("session-no-plan");
  if (oldNote) oldNote.remove();

  try {
    logPlan = await api.get("/api/my/plan");
  } catch (err) {
    if (err.status === 404) {
      logPlan = null;
      form.classList.add("hidden");
      const note = document.createElement("p");
      note.id = "session-no-plan";
      note.className = "panel-placeholder";
      note.textContent = "You need an active plan before you can log a session.";
      panel.insertBefore(note, form);
      document.getElementById("session-history").textContent = "";
      return;
    }
    showToast(err.detail || err.title || "Could not load your plan", true);
    return;
  }

  form.classList.remove("hidden");
  form.reset();
  resetSessionEditState();

  const dayField = document.getElementById("session-day-field");
  const daySelect = form.elements.day;
  const labels = [];
  logPlan.items.forEach((item) => {
    if (item.day_label && labels.indexOf(item.day_label) === -1) {
      labels.push(item.day_label);
    }
  });
  daySelect.textContent = "";
  if (labels.length > 0) {
    labels.forEach((label) => {
      const option = document.createElement("option");
      option.value = label;
      option.textContent = label;
      daySelect.appendChild(option);
    });
    dayField.classList.remove("hidden");
  } else {
    dayField.classList.add("hidden");
  }

  renderSessionExercises();
  loadSessionHistory();
}

function resetSessionEditState() {
  editingSessionId = null;
  document.getElementById("session-submit").textContent = "Log session";
  document.getElementById("session-cancel").classList.add("hidden");
}

function makeSetInput(className, placeholder) {
  const el = document.createElement("input");
  el.type = "number";
  el.className = className;
  el.placeholder = placeholder;
  el.min = "0";
  return el;
}

// `source` describes one set row: exercise_id, plan_item_id (or null), and any
// of reps / weight / rpe / completed to prefill. Used both for a fresh log
// (prefilled from the plan's targets) and for editing (prefilled from the
// logged set).
function makeSetRow(source) {
  const row = document.createElement("div");
  row.className = "set-row";
  row.dataset.exerciseId = String(source.exercise_id);
  row.dataset.planItemId =
    source.plan_item_id != null ? String(source.plan_item_id) : "";

  const reps = makeSetInput("set-reps", "Reps");
  if (source.reps != null) reps.value = source.reps;
  row.appendChild(reps);

  const weight = makeSetInput("set-weight", "Weight");
  weight.step = "0.5";
  if (source.weight != null) weight.value = source.weight;
  row.appendChild(weight);

  const rpe = makeSetInput("set-rpe", "RPE");
  rpe.max = "10";
  rpe.step = "0.5";
  if (source.rpe != null) rpe.value = source.rpe;
  row.appendChild(rpe);

  const done = document.createElement("label");
  done.className = "set-done";
  const check = document.createElement("input");
  check.type = "checkbox";
  check.className = "set-completed";
  check.checked = source.completed != null ? source.completed : true;
  done.appendChild(check);
  done.appendChild(document.createTextNode(" done"));
  row.appendChild(done);

  const remove = document.createElement("button");
  remove.type = "button";
  remove.className = "ghost";
  remove.textContent = "×";
  remove.addEventListener("click", () => row.remove());
  row.appendChild(remove);

  return row;
}

// existingSets != null => build the form from a logged session (edit mode);
// otherwise build it fresh from the active plan's prescribed exercises.
function renderSessionExercises(existingSets) {
  const container = document.getElementById("session-exercises");
  container.textContent = "";

  let blocks;
  if (existingSets) {
    const order = [];
    const byExercise = {};
    existingSets.forEach((set) => {
      if (!byExercise[set.exercise_id]) {
        byExercise[set.exercise_id] = [];
        order.push(set.exercise_id);
      }
      byExercise[set.exercise_id].push(set);
    });
    blocks = order.map((exId) => ({
      name: byExercise[exId][0].exercise_name,
      rows: byExercise[exId],
      template: {
        exercise_id: exId,
        plan_item_id: byExercise[exId][0].plan_item_id,
        completed: true,
      },
    }));
  } else {
    const dayField = document.getElementById("session-day-field");
    const usingDays = !dayField.classList.contains("hidden");
    const selectedDay = usingDays
      ? document.getElementById("session-form").elements.day.value
      : null;
    const items = logPlan.items.filter(
      (item) => !usingDays || (item.day_label || "") === selectedDay
    );
    blocks = items.map((item) => {
      const template = {
        exercise_id: item.exercise_id,
        plan_item_id: item.id,
        reps: item.target_reps,
        weight: item.target_weight,
        completed: true,
      };
      const count =
        item.target_sets && item.target_sets > 0 ? item.target_sets : 1;
      const rows = [];
      for (let i = 0; i < count; i++) rows.push(template);
      return { name: item.exercise_name, rows: rows, template: template };
    });
  }

  blocks.forEach((block) => {
    const div = document.createElement("div");
    div.className = "session-exercise";

    const heading = document.createElement("h4");
    heading.textContent = block.name;
    div.appendChild(heading);

    const head = document.createElement("div");
    head.className = "set-head";
    ["Reps", "Weight (kg)", "RPE 0–10"].forEach((label) => {
      const span = document.createElement("span");
      span.textContent = label;
      head.appendChild(span);
    });
    div.appendChild(head);

    const setRows = document.createElement("div");
    setRows.className = "set-rows";
    block.rows.forEach((source) => setRows.appendChild(makeSetRow(source)));
    div.appendChild(setRows);

    const addSet = document.createElement("button");
    addSet.type = "button";
    addSet.className = "ghost";
    addSet.textContent = "+ set";
    addSet.addEventListener("click", () =>
      setRows.appendChild(makeSetRow(block.template))
    );
    div.appendChild(addSet);

    container.appendChild(div);
  });
}

function readSessionSets() {
  const rows = document.querySelectorAll("#session-exercises .set-row");
  const sets = [];
  rows.forEach((row) => {
    const set = {
      exercise_id: Number(row.dataset.exerciseId),
      completed: row.querySelector(".set-completed").checked,
    };
    if (row.dataset.planItemId) {
      set.plan_item_id = Number(row.dataset.planItemId);
    }
    const reps = row.querySelector(".set-reps").value;
    const weight = row.querySelector(".set-weight").value;
    const rpe = row.querySelector(".set-rpe").value;
    if (reps) set.reps = Number(reps);
    if (weight) set.weight = Number(weight);
    if (rpe) set.rpe = Number(rpe);
    sets.push(set);
  });
  return sets;
}

async function handleSessionSubmit(event) {
  event.preventDefault();
  const form = event.target;

  const sets = readSessionSets();
  if (sets.length === 0) {
    showToast("Add at least one set", true);
    return;
  }

  const status = form.elements.status.value;
  const notes = form.elements.notes.value.trim();

  try {
    if (editingSessionId) {
      await api.patch("/api/my/sessions/" + editingSessionId, {
        status: status,
        notes: notes || null,
        sets: sets,
      });
      showToast("Session updated");
    } else {
      const payload = { plan_id: logPlan.id, status: status, sets: sets };
      if (notes) payload.notes = notes;
      const performedOn = form.elements.performed_on.value;  // "YYYY-MM-DD" or ""
      if (performedOn) payload.performed_at = performedOn + " 12:00:00";
      await api.post("/api/my/sessions", payload);
      showToast("Session logged");
    }
    loadLogSession();
  } catch (err) {
    showToast(err.detail || err.title || "Could not save the session", true);
  }
}

function startSessionEdit(session) {
  const form = document.getElementById("session-form");
  form.elements.status.value = session.status;
  form.elements.notes.value = session.notes || "";
  form.elements.performed_on.value = (session.performed_at || "").slice(0, 10);

  document.getElementById("session-day-field").classList.add("hidden");
  renderSessionExercises(session.sets);

  editingSessionId = session.id;
  document.getElementById("session-submit").textContent = "Save changes";
  document.getElementById("session-cancel").classList.remove("hidden");
  form.scrollIntoView();
}

async function handleSessionDelete(session) {
  if (!window.confirm("Delete this session?")) {
    return;
  }
  try {
    await api.del("/api/my/sessions/" + session.id);
    showToast("Session deleted");
    if (editingSessionId === session.id) {
      loadLogSession();
    } else {
      loadSessionHistory();
    }
  } catch (err) {
    showToast(err.detail || err.title || "Could not delete the session", true);
  }
}

async function loadSessionHistory() {
  const list = document.getElementById("session-history");
  try {
    const data = await api.get("/api/my/sessions");
    const recent = data.sessions.slice(0, 5);

    list.textContent = "";

    if (recent.length === 0) {
      const empty = document.createElement("li");
      empty.className = "item-empty";
      empty.textContent = "No sessions logged yet.";
      list.appendChild(empty);
      return;
    }

    recent.forEach((session) => {
      const li = document.createElement("li");
      li.className = "item session-row";

      const main = document.createElement("div");
      main.className = "session-row-main";

      const when = document.createElement("span");
      when.className = "item-title";
      when.textContent = (session.performed_at || "").slice(0, 10) || "—";
      main.appendChild(when);

      const meta = document.createElement("span");
      meta.className = "item-sub";
      meta.textContent =
        session.status + "   ·   " + session.sets.length + " sets";
      main.appendChild(meta);

      const actions = document.createElement("span");
      actions.className = "item-actions";

      const detail = document.createElement("ul");
      detail.className = "session-detail hidden";
      session.sets.forEach((set) => {
        const line = document.createElement("li");
        const bits = [set.exercise_name];
        if (set.reps != null && set.weight != null) {
          bits.push(set.reps + " × " + set.weight);
        } else if (set.reps != null) {
          bits.push(set.reps + " reps");
        }
        if (set.rpe != null) bits.push("RPE " + set.rpe);
        if (!set.completed) bits.push("skipped");
        line.textContent = "Set " + set.set_number + ":  " + bits.join("   ·   ");
        detail.appendChild(line);
      });

      const detailsBtn = document.createElement("button");
      detailsBtn.type = "button";
      detailsBtn.className = "ghost";
      detailsBtn.textContent = "Details";
      detailsBtn.addEventListener("click", () =>
        detail.classList.toggle("hidden")
      );
      actions.appendChild(detailsBtn);

      const editBtn = document.createElement("button");
      editBtn.type = "button";
      editBtn.className = "ghost";
      editBtn.textContent = "Edit";
      editBtn.addEventListener("click", () => startSessionEdit(session));
      actions.appendChild(editBtn);

      const deleteBtn = document.createElement("button");
      deleteBtn.type = "button";
      deleteBtn.className = "ghost";
      deleteBtn.textContent = "Delete";
      deleteBtn.addEventListener("click", () => handleSessionDelete(session));
      actions.appendChild(deleteBtn);

      main.appendChild(actions);
      li.appendChild(main);
      li.appendChild(detail);
      list.appendChild(li);
    });
  } catch (err) {
    showToast(err.detail || err.title || "Could not load your history", true);
  }
}

// --- trainee: notes ----------------------------------------
let notesPlan = null;         // the active plan, for the exercise list per day
let notesByExercise = {};     // exercise_id -> saved note body

async function loadNotes() {
  const panel = document.getElementById("panel-notes");
  panel.textContent = "";

  let plan;
  let noteList;
  try {
    [plan, noteList] = await Promise.all([
      api.get("/api/my/plan"),
      api.get("/api/my/notes"),
    ]);
  } catch (err) {
    if (err.status === 404) {
      const note = document.createElement("p");
      note.className = "panel-placeholder";
      note.textContent = "No plan has been assigned to you yet.";
      panel.appendChild(note);
      return;
    }
    showToast(err.detail || err.title || "Could not load your notes", true);
    return;
  }

  notesPlan = plan;
  notesByExercise = {};
  noteList.notes.forEach((n) => {
    notesByExercise[n.exercise_id] = n.body;
  });

  const labels = [];
  plan.items.forEach((item) => {
    if (item.day_label && labels.indexOf(item.day_label) === -1) {
      labels.push(item.day_label);
    }
  });
  if (labels.length > 0) {
    const wrap = document.createElement("label");
    wrap.className = "notes-day";
    wrap.appendChild(document.createTextNode("Day "));
    const select = document.createElement("select");
    select.id = "notes-day-select";
    labels.forEach((label) => {
      const option = document.createElement("option");
      option.value = label;
      option.textContent = label;
      select.appendChild(option);
    });
    select.addEventListener("change", renderNotes);
    wrap.appendChild(select);
    panel.appendChild(wrap);
  }

  const container = document.createElement("div");
  container.id = "notes-list";
  panel.appendChild(container);
  renderNotes();
}

function renderNotes() {
  const container = document.getElementById("notes-list");
  container.textContent = "";

  const daySelect = document.getElementById("notes-day-select");
  const selectedDay = daySelect ? daySelect.value : null;

  const items = notesPlan.items.filter(
    (item) => !daySelect || (item.day_label || "") === selectedDay
  );

  const seen = {};
  items.forEach((item) => {
    if (seen[item.exercise_id]) return;
    seen[item.exercise_id] = true;

    const block = document.createElement("div");
    block.className = "note-block";

    const heading = document.createElement("h4");
    heading.textContent = item.exercise_name;
    block.appendChild(heading);

    const area = document.createElement("textarea");
    area.className = "note-body";
    area.rows = 3;
    area.placeholder = "Cues from your coach…";
    area.value = notesByExercise[item.exercise_id] || "";
    block.appendChild(area);

    const actions = document.createElement("div");
    actions.className = "form-actions";

    const save = document.createElement("button");
    save.type = "button";
    save.textContent = "Save";
    save.addEventListener("click", () =>
      handleNoteSave(item.exercise_id, area.value)
    );
    actions.appendChild(save);

    const clear = document.createElement("button");
    clear.type = "button";
    clear.className = "ghost";
    clear.textContent = "Clear";
    clear.addEventListener("click", () => handleNoteClear(item.exercise_id));
    actions.appendChild(clear);

    block.appendChild(actions);
    container.appendChild(block);
  });
}

async function handleNoteSave(exerciseId, body) {
  const trimmed = body.trim();
  if (!trimmed) {
    showToast("Write something first, or use Clear", true);
    return;
  }
  try {
    await api.put("/api/my/notes/" + exerciseId, { body: trimmed });
    notesByExercise[exerciseId] = trimmed;
    showToast("Note saved");
  } catch (err) {
    showToast(err.detail || err.title || "Could not save the note", true);
  }
}

async function handleNoteClear(exerciseId) {
  if (!notesByExercise[exerciseId]) {
    renderNotes();  // nothing saved yet, just reset the field
    return;
  }
  if (!window.confirm("Delete this note?")) {
    return;
  }
  try {
    await api.del("/api/my/notes/" + exerciseId);
    delete notesByExercise[exerciseId];
    showToast("Note deleted");
    renderNotes();
  } catch (err) {
    showToast(err.detail || err.title || "Could not delete the note", true);
  }
}

// --- trainee: my progress ----------------------------------
let progressCharts = [];  // live Chart instances, torn down before each redraw

function destroyProgressCharts() {
  progressCharts.forEach((chart) => chart.destroy());
  progressCharts = [];
}

function lineChart(canvas, labels, values, label) {
  const chart = new Chart(canvas, {
    type: "line",
    data: {
      labels: labels,
      datasets: [
        {
          label: label,
          data: values,
          borderColor: "#2f6feb",
          backgroundColor: "rgba(47, 111, 235, 0.12)",
          tension: 0.25,
          fill: true,
          pointRadius: 3,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: { y: { beginAtZero: true } },
    },
  });
  progressCharts.push(chart);
}

function statTile(value, label) {
  const tile = document.createElement("div");
  tile.className = "stat";

  const v = document.createElement("span");
  v.className = "stat-value";
  v.textContent = value;
  tile.appendChild(v);

  const l = document.createElement("span");
  l.className = "stat-label";
  l.textContent = label;
  tile.appendChild(l);

  return tile;
}

function addChartSection(parent, title, series, label) {
  const section = document.createElement("div");
  section.className = "chart-section";

  const heading = document.createElement("h4");
  heading.textContent = title;
  section.appendChild(heading);

  const wrap = document.createElement("div");
  wrap.className = "chart-wrap";
  const canvas = document.createElement("canvas");
  wrap.appendChild(canvas);
  section.appendChild(wrap);

  parent.appendChild(section);  // in the DOM before the chart is created
  lineChart(
    canvas,
    series.map((p) => p.date),
    series.map((p) => p.value),
    label
  );
}

async function loadMyProgress() {
  const panel = document.getElementById("panel-my-progress");
  destroyProgressCharts();
  panel.textContent = "";

  let report;
  try {
    report = await api.get("/api/my/progress");
  } catch (err) {
    showToast(err.detail || err.title || "Could not load your progress", true);
    return;
  }

  const stats = document.createElement("div");
  stats.className = "stat-row";
  stats.appendChild(
    statTile(Math.round(report.total_volume).toLocaleString() + " kg", "Total volume")
  );
  stats.appendChild(
    statTile(Math.round(report.adherence * 100) + "%", "Adherence")
  );
  stats.appendChild(
    statTile(
      report.weekly_streak +
        (report.weekly_streak === 1 ? " week" : " weeks"),
      "Weekly streak"
    )
  );
  panel.appendChild(stats);

  const hasVolume = report.volume_over_time.length > 0;
  const hasE1rm = report.exercises.length > 0;

  if (!hasVolume && !hasE1rm) {
    const note = document.createElement("p");
    note.className = "panel-placeholder";
    note.textContent = "Log a session and your charts will appear here.";
    panel.appendChild(note);
    return;
  }

  if (hasVolume) {
    addChartSection(
      panel,
      "Volume over time (kg)",
      report.volume_over_time,
      "Volume"
    );
  }
  report.exercises.forEach((ex) => {
    addChartSection(
      panel,
      "Best est. 1RM — " + ex.exercise_name,
      ex.best_e1rm_over_time,
      ex.exercise_name
    );
  });
}

// --- tabs --------------------------------------------------
function selectTab(name) {
  const showLogin = name === "login";
  document.getElementById("show-login").classList.toggle("active", showLogin);
  document.getElementById("show-register").classList.toggle("active", !showLogin);
  document.getElementById("login-form").classList.toggle("hidden", !showLogin);
  document.getElementById("register-form").classList.toggle("hidden", showLogin);
}

// --- startup ---------------------------------------------
async function start() {
  document
    .getElementById("show-login")
    .addEventListener("click", () => selectTab("login"));
  document
    .getElementById("show-register")
    .addEventListener("click", () => selectTab("register"));
  document
    .getElementById("login-form")
    .addEventListener("submit", handleLogin);
  document
    .getElementById("register-form")
    .addEventListener("submit", handleRegister);
  document
    .getElementById("logout-btn")
    .addEventListener("click", logout);
  document.querySelectorAll("#google-role .role-choice button").forEach((btn) => {
    btn.addEventListener("click", () => submitGoogleRole(btn.dataset.role));
  });

  document.querySelectorAll("#coach-view .dash-tab").forEach((tab) => {
    tab.addEventListener("click", () => selectCoachPanel(tab.dataset.panel));
  });
  document.querySelectorAll("#trainee-view .dash-tab").forEach((tab) => {
    tab.addEventListener("click", () => selectTraineePanel(tab.dataset.panel));
  });
  document
    .getElementById("session-form")
    .addEventListener("submit", handleSessionSubmit);
  document
    .getElementById("session-form")
    .elements.day.addEventListener("change", () => renderSessionExercises());
  document
    .getElementById("session-cancel")
    .addEventListener("click", () => loadLogSession());
  document
    .getElementById("exercise-form")
    .addEventListener("submit", handleExerciseSubmit);
  document
    .getElementById("exercise-cancel")
    .addEventListener("click", resetExerciseForm);
  document
    .getElementById("trainee-form")
    .addEventListener("submit", handleTraineeSubmit);
  document
    .getElementById("plan-form")
    .addEventListener("submit", handlePlanSubmit);
  document
    .getElementById("plan-add-item")
    .addEventListener("click", () => addPlanItemRow());
  document
    .getElementById("plan-cancel")
    .addEventListener("click", resetPlanForm);

  api.onUnauthorized(() => {
    showAuthView();
    showToast("Your session expired. Please log in again.", true);
  });

  await loadAuthConfig();

  const token = api.getToken();
  const cachedUser = api.getUser();

  if (token && cachedUser) {
    showDashboard(cachedUser);
  }

  if (token) {
    try {
      const user = await api.get("/api/auth/me");
      api.setUser(user);
      showDashboard(user);
    } catch (err) {
      // handled by onUnauthorized
    }
    return;
  }

  api.clearUser();
  showAuthView();
}

document.addEventListener("DOMContentLoaded", start);