document.addEventListener('DOMContentLoaded', async () => {
    const roomsGrid = document.getElementById('rooms-grid');
    if (!roomsGrid) return;
    const filterForm = document.getElementById('filter-form');
    const filterLoc = document.getElementById('filter-location');
    const filterType = document.getElementById('filter-type');
    const filterPrice = document.getElementById('filter-price');
    let rooms = [];
    let favorites = [];
    const params = new URLSearchParams(window.location.search);
    if (params.has('location')) filterLoc.value = params.get('location');
    if (params.has('type')) filterType.value = params.get('type');

    function renderRooms() {
        const location = filterLoc.value.toLowerCase().trim();
        const type = filterType.value;
        const maxPrice = filterPrice.value ? Number(filterPrice.value) : Infinity;
        const filtered = rooms.filter(room => {
            const matchLocation = [room.location, room.city, room.title].some(value =>
                String(value || '').toLowerCase().includes(location));
            return matchLocation && (type === '' || room.type === type) && Number(room.price) <= maxPrice;
        });
        if (filtered.length === 0) {
            roomsGrid.innerHTML = '<div class="no-rooms">No accommodations found matching your filters.</div>';
            return;
        }
        roomsGrid.innerHTML = filtered.map(room => roomCardHTML(room, favorites.includes(room.id))).join('');
        roomsGrid.querySelectorAll('.fav-btn').forEach(button => {
            button.addEventListener('click', async () => {
                button.disabled = true;
                try {
                    const updated = await toggleFavorite(Number(button.dataset.id));
                    if (updated) {
                        favorites = await getFavorites();
                        renderRooms();
                    }
                } catch (err) {
                    alert(err.message);
                } finally {
                    button.disabled = false;
                }
            });
        });
    }
    filterForm.addEventListener('submit', event => event.preventDefault());
    try {
        rooms = await getRooms();
        favorites = await getFavorites();
        renderRooms();
        filterLoc.addEventListener('input', renderRooms);
        filterType.addEventListener('change', renderRooms);
        filterPrice.addEventListener('change', renderRooms);
    } catch (err) {
        console.error('Error loading rooms:', err);
        roomsGrid.innerHTML = `<div class="no-rooms">${escapeHTML(err.message)}</div>`;
    }
});
