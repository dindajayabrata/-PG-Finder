document.addEventListener('DOMContentLoaded', () => {
    const form = document.getElementById('contact-form');
    if (!form) return;
    form.addEventListener('submit', async event => {
        event.preventDefault();
        const contactData = {
            name: document.getElementById('contact-name').value.trim(),
            email: document.getElementById('contact-email').value.trim(),
            subject: document.getElementById('contact-subject').value.trim(),
            message: document.getElementById('contact-message').value.trim()
        };
        if (!contactData.name || !contactData.message) {
            alert('Enter your name and message.');
            return;
        }
        const button = form.querySelector('button[type="submit"]');
        button.disabled = true;
        try {
            await apiRequest('/contact', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(contactData)
            });
            alert('Thank you! Your message has been saved.');
            form.reset();
        } catch (err) {
            alert(err.message);
        } finally {
            button.disabled = false;
        }
    });
});
