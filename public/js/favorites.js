document.addEventListener('DOMContentLoaded', () => {
    const grid = document.getElementById('favorites-grid');
    if (!grid) return;
    if (!getCurrentUser()) {
        grid.innerHTML = '<div class="no-rooms">Please <a href="login.html">log in</a> to view your saved rooms.</div>';
        return;
    }
    async function renderFavorites() {
        try {
            const rooms = await getRooms();
            const favorites = await getFavorites();
            const saved = rooms.filter(room => favorites.includes(room.id));
            if (saved.length === 0) {
                grid.innerHTML = '<div class="no-rooms">You haven\'t saved any rooms yet. <a href="rooms.html">Browse rooms</a> and click the heart button to save one.</div>';
                return;
            }
            grid.innerHTML = saved.map(room => roomCardHTML(room, true)).join('');
            grid.querySelectorAll('.fav-btn').forEach(button => {
                button.addEventListener('click', async () => {
                    button.disabled = true;
                    try {
                        const updated = await toggleFavorite(Number(button.dataset.id));
                        if (updated) await renderFavorites();
                    } finally {
                        button.disabled = false;
                    }
                });
            });
        } catch (err) {
            console.error('Error loading favorites:', err);
            grid.innerHTML = `<div class="no-rooms">${escapeHTML(err.message)}</div>`;
        }
    }
    renderFavorites();
});
