document.addEventListener('DOMContentLoaded', () => {
    const form = document.getElementById('add-room-form');
    if (!form) return;
    form.addEventListener('submit', async event => {
        event.preventDefault();
        const user = getCurrentUser();
        if (!user) {
            alert('Please log in before adding a room.');
            window.location.href = 'login.html';
            return;
        }
        const newRoom = {
            title: document.getElementById('room-title').value.trim(),
            location: document.getElementById('room-location').value.trim(),
            price: Number(document.getElementById('room-price').value),
            type: document.getElementById('room-type').value,
            image: document.getElementById('room-image').value.trim(),
            desc: document.getElementById('room-desc').value.trim(),
            ownerId: user.id
        };
        if (!newRoom.title || !newRoom.location) {
            alert('Enter a property title and location.');
            return;
        }
        if (!Number.isFinite(newRoom.price) || newRoom.price < 0.01) {
            alert('Monthly rent must be greater than zero.');
            return;
        }
        if (!safeImageUrl(newRoom.image)) {
            alert('Enter an image URL beginning with http:// or https://.');
            return;
        }
        const button = form.querySelector('button[type="submit"]');
        button.disabled = true;
        try {
            await apiRequest('/rooms', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(newRoom)
            });
            alert('Property listed successfully!');
            window.location.href = 'rooms.html';
        } catch (err) {
            alert(err.message);
        } finally {
            button.disabled = false;
        }
    });
});
